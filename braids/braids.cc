// Copyright 2012 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.

#include <stm32f10x_conf.h>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/ring_buffer.h"
#include "stmlib/system/system_clock.h"
#include "stmlib/system/uid.h"

#include "braids/drivers/adc.h"
#include "braids/drivers/dac.h"
#include "braids/drivers/debug_pin.h"
#include "braids/drivers/gate_input.h"
#include "braids/drivers/internal_adc.h"
#include "braids/drivers/system.h"
#include "braids/envelope.h"
#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "braids/ui.h"

using namespace braids;
using namespace stmlib;

const uint16_t kAudioBufferSize = 128;
const uint16_t kAudioBlockSize = 24;

RingBuffer<uint16_t, kAudioBufferSize> audio_samples;
RingBuffer<uint8_t, kAudioBufferSize> sync_samples;
MacroOscillator osc;
Envelope envelope;  // first envelope/LFO for timbre modulation.
Envelope envelope2; // second envelope/LFO instance for color modulation.
// Envelope envelope3; // third envelope/LFO instance for level modulation.
Adc adc;
Dac dac;
DebugPin debug_pin;
GateInput gate_input;
InternalAdc internal_adc;
SignatureWaveshaper ws;
System sys;
VcoJitterSource jitter_source;
Ui ui;

int16_t render_buffer[kAudioBlockSize];
uint8_t sync_buffer[kAudioBlockSize];

bool trigger_detected_flag;
volatile bool trigger_flag;
uint16_t trigger_delay;

extern "C" {
  
void HardFault_Handler(void) { while (1); }
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void) { while (1); }
void UsageFault_Handler(void) { while (1); }
void NMI_Handler(void) { }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }

}

extern "C" {

void SysTick_Handler() {
  system_clock.Tick();  // Tick global ms counter.
  ui.Poll();
}

void TIM1_UP_IRQHandler(void) {
  if (TIM_GetITStatus(TIM1, TIM_IT_Update) == RESET) {
    return;
  }
  
  TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
  
  dac.Write(audio_samples.ImmediateRead());

  bool trigger_detected = gate_input.raised();
  sync_samples.Overwrite(trigger_detected);
  trigger_detected_flag = trigger_detected_flag | trigger_detected;
  
  bool adc_scan_cycle_complete = adc.PipelinedScan();
  if (adc_scan_cycle_complete) {
    ui.UpdateCv(adc.channel(0), adc.channel(1), adc.channel(2), adc.channel(3));
    if (trigger_detected_flag) {
      trigger_delay = settings.trig_delay()
          ? (1 << settings.trig_delay()) : 0;
      ++trigger_delay;
      trigger_detected_flag = false;
    }
    if (trigger_delay) {
      --trigger_delay;
      if (trigger_delay == 0) {
        trigger_flag = true;
      }
    }
  }
}

}

void Init() {
  sys.Init(F_CPU / 96000 - 1, true);
  settings.Init();
  ui.Init();
  system_clock.Init();
  adc.Init(false);
  gate_input.Init();
  debug_pin.Init();
  dac.Init();
  osc.Init();
  audio_samples.Init();
  sync_samples.Init();
  internal_adc.Init();
  
  for (uint16_t i = 0; i < kAudioBufferSize / 2; ++i) {
    sync_samples.Overwrite(0);
    audio_samples.Overwrite(0);
  }
  
  envelope.Init();
  envelope2.Init();
  // envelope3.Init();
  ws.Init(GetUniqueId(2));
  jitter_source.Init(GetUniqueId(1));
  sys.StartTimers();
}

const uint16_t bit_reduction_masks[] = {
    0xc000,
    0xe000,
    0xf000,
    0xf800,
    0xff00,
    0xfff0,
    0xffff };

const uint16_t decimation_factors[] = { 24, 12, 6, 4, 3, 2, 1 };

void RenderBlock() {
  static uint16_t previous_pitch_adc_code = 0;
  static int32_t previous_pitch = 0;
  static int32_t previous_shape = 0;

  //debug_pin.High();

  uint8_t meta_mod = settings.meta_modulation();
  
  // use FM CV data for env params if envelopes or LFO modes are enabled
  // Note, we invert the parameter if in LFO mode, so higher voltages produce 
  // higher LFO frequencies
  uint16_t env_param = uint16_t (settings.mod1_rate());
  uint16_t env_a = 0;
  uint16_t env_d = 0;
  uint8_t modulator1_mode = settings.mod1_mode();
  // add the external voltage to this.
  // scaling this by 32 seems about right for 0-5V modulation range.
  if (meta_mod == 2 || meta_mod == 3) {
     env_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }
  // Clip at zero and 127
  if (env_param < 0) {
 	 env_param = 0 ;
  }
  if (env_param > 127) {
	 env_param = 127 ;
  } 
  // Invert if in LFO mode, so higher CVs create higher LFO frequency.
  if (modulator1_mode == 2) {
	 env_param = 127 - env_param ;
  }  
  // attack and decay parameters, default to FM voltage reading.
  env_a = env_param;
  env_d = env_param;
  // These are ratios of attack to decay, from A/D = 0.02 
  // through to A/D=0.9, then A=D, then D/A = 0.9 down to 0.02
  // as listed in ad_ratio_values in settings.cc
  uint8_t modulator1_ad_ratio = settings.mod1_ad_ratio();
  if (modulator1_ad_ratio == 0) {
   env_a = (env_param * 2) / 100; 
  } 
  else if (modulator1_ad_ratio > 0 && modulator1_ad_ratio < 10) {
    env_a = (env_param * 10 * modulator1_ad_ratio) / 100; 
  } 
  else if (modulator1_ad_ratio > 10 && modulator1_ad_ratio < 20) {
    env_d = (env_param * 10 * (20 - modulator1_ad_ratio)) / 100; 
  } 
  else if (modulator1_ad_ratio == 20) {
    env_d = (env_param * 2) / 100; 
  } 
  // now set the attack and decay parameters again
  // using the modified attack and decay values
  envelope.Update(env_a, env_d, 0, 0);  

  // TO-DO: instead of repeating code, use an array for env params and a loop!
  uint16_t env2_param = uint16_t (settings.mod2_rate());
  uint16_t env2_a = 0;
  uint16_t env2_d = 0;
  uint8_t modulator2_mode = settings.mod2_mode();
  // add the external voltage to this.
  // scaling this by 32 seems about right for 0-5V modulation range.
  if (meta_mod == 2 || meta_mod == 4) {
     env2_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }
  if (env2_param < 0) { 
 	 env2_param = 0 ;
  }
  if (env2_param > 127) {
 	 env2_param = 127 ;
  } 
  // Invert if in LFO mode, so higher CVs create higher LFO frequency.
  if (modulator2_mode == 2) { 
	 env2_param = 127 - env2_param ;
  }  
  env2_a = env2_param;
  env2_d = env2_param;
  // Repeat for envelope2
  uint8_t modulator2_ad_ratio = settings.mod2_ad_ratio();
  if (modulator2_ad_ratio == 0) {
  	env2_a = (env2_param * 2) / 100; 
  } 
  else if (modulator2_ad_ratio > 0 && modulator2_ad_ratio < 10) {
    env2_a = (env2_param * 10 * modulator2_ad_ratio) / 100; 
  } 
  else if (modulator2_ad_ratio > 10 && modulator2_ad_ratio < 20) {
    env2_d = (env2_param * 10 * (20 - modulator2_ad_ratio)) / 100; 
  } 
  else if (modulator2_ad_ratio == 20) {
    env2_d = (env2_param * 2) / 100; 
  }     
  // now set the attack and decay parameters again
  // using the modified attack and decay values
  envelope2.Update(env2_a, env2_d, 0, 0);  
  
  // Render envelope in LFO mode, or not
  // envelope 1
  uint8_t modulator1_destination = settings.mod1_destination();
  uint8_t modulator1_shape = settings.mod1_shape();
  uint16_t ad_value = 0 ;
  if (modulator1_mode == 2) { 
      // LFO mode
      ad_value = envelope.Render(true, modulator1_shape);
  }
  else if (modulator1_mode == 1){
      // envelope mode
      ad_value = envelope.Render(false, modulator1_shape);
  }
  // envelope 2
  uint8_t modulator2_destination = settings.mod2_destination();
  uint8_t modulator2_shape = settings.mod2_shape();
  uint16_t ad2_value = 0 ;
  if (modulator2_mode == 2) { 
      // LFO mode
      ad2_value = envelope2.Render(true, modulator2_shape);
  }
  else if (modulator2_mode == 1) {
      // envelope mode
      ad2_value = envelope2.Render(false, modulator2_shape);
  }

  // timbre modulation amount - sum the two modulators
  uint8_t modulator1_depth = settings.mod1_depth();
  uint8_t modulator2_depth = settings.mod2_depth();
  uint16_t ad_timbre_amount=0;
  if (modulator1_mode && (modulator1_destination & 1)) {
      ad_timbre_amount = modulator1_depth ;
  } 
  if (modulator2_mode && (modulator2_destination & 1)) {
      ad_timbre_amount += modulator2_depth ;
  } 
  if (ad_timbre_amount > 255) {
     ad_timbre_amount = 255 ;
  }
      
  // color modulation amount
  uint16_t ad_color_amount=0;
  if (modulator1_mode && (modulator1_destination & 4)) {
      ad_color_amount = modulator1_depth ;
  } 
  if (modulator2_mode && (modulator2_destination & 4)) {
      ad_color_amount += modulator2_depth ;
  } 
  if (ad_color_amount > 255) {
     ad_color_amount = 255 ;
  }

  // level modulation amount
  uint16_t ad_level_amount=255;
  if (modulator1_mode == 2 && (modulator1_destination & 2)) {
      ad_level_amount = modulator1_depth ;
  } 
  if (modulator2_mode == 2 && (modulator2_destination & 2)) {
      ad_level_amount += modulator2_depth ;
  } 
  if (ad_level_amount > 255) {
     ad_level_amount = 255 ;
  }
   
  // meta_modulation no longer a boolean  
  if (meta_mod == 1) {
    int32_t shape = adc.channel(3);
    shape -= settings.data().fm_cv_offset;
    if (shape > previous_shape + 2 || shape < previous_shape - 2) {
      previous_shape = shape;
    } else {
      shape = previous_shape;
    }
    shape = MACRO_OSC_SHAPE_LAST * shape >> 11;
    shape += settings.shape();
    if (shape >= MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META) {
        shape = MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META;
    } else if (shape <= 0) {
      shape = 0;
    }
    MacroOscillatorShape osc_shape = static_cast<MacroOscillatorShape>(shape);
    osc.set_shape(osc_shape);
    ui.set_meta_shape(osc_shape);
  } else {
    osc.set_shape(settings.shape());
  }
  // modulate timbre
  uint16_t parameter_1 = adc.channel(0) << 3;
  // Need to up-cast the envelope values because we will be summing them 
  // and we want to clip them at 65535, rather than having the sum overflow. 
  uint32_t adv1 = static_cast<uint32_t>(ad_value);
  uint32_t adv2 = static_cast<uint32_t>(ad2_value);
  uint32_t tad_value=0;
  uint32_t cad_value=0;
  if (modulator1_destination & 1) {
      tad_value = adv1;
  }
  if (modulator2_destination & 1) {
      tad_value += adv2;
  }
  if (tad_value > 65535) {
      tad_value = 65535;
  }
  parameter_1 += tad_value * ad_timbre_amount >> 9;
  if (parameter_1 > 32767) {
    parameter_1 = 32767;
  }
  // modulate colour
  uint16_t parameter_2 = adc.channel(1) << 3;
  if (modulator1_destination & 4) {
      cad_value = adv1;
  }
  if (modulator2_destination & 4) {
      cad_value += adv2;
  }
  if (cad_value > 65535) {
      cad_value = 65535;
  }
  parameter_2 += cad_value * ad_color_amount >> 9;
  if (parameter_2 > 32767) {
    parameter_2 = 32767;
  }
  osc.set_parameters(parameter_1, parameter_2);
    
  // Apply hysteresis to ADC reading to prevent a single bit error to move
  // the quantized pitch up and down the quantization boundary.
  uint16_t pitch_adc_code = adc.channel(2);
  if (settings.pitch_quantization()) {
    if ((pitch_adc_code > previous_pitch_adc_code + 4) ||
        (pitch_adc_code < previous_pitch_adc_code - 4)) {
      previous_pitch_adc_code = pitch_adc_code;
    } else {
      pitch_adc_code = previous_pitch_adc_code;
    }
  }
  int32_t pitch = settings.adc_to_pitch(pitch_adc_code);
  if (settings.pitch_quantization() == PITCH_QUANTIZATION_QUARTER_TONE) {
    pitch = (pitch + 32) & 0xffffffc0;
  } else if (settings.pitch_quantization() == PITCH_QUANTIZATION_SEMITONE) {
    pitch = (pitch + 64) & 0xffffff80;
  }
  if (meta_mod == 0) {
    pitch += settings.adc_to_fm(adc.channel(3));
  }
  pitch += internal_adc.value() >> 8;
  
  // Check if the pitch has changed to cause an auto-retrigger
  int32_t pitch_delta = pitch - previous_pitch;
  if (settings.data().auto_trig &&
      (pitch_delta >= 0x40 || -pitch_delta >= 0x40)) {
    trigger_detected_flag = true;
  }
  previous_pitch = pitch;
  
  if (settings.vco_drift()) {
    int16_t jitter = jitter_source.Render(adc.channel(1) << 3);
    pitch += (jitter >> 8);
  }

  if (pitch > 32767) {
    pitch = 32767;
  } else if (pitch < 0) {
    pitch = 0;
  }
  
  if (settings.vco_flatten()) {
    if (pitch > 16383) {
      pitch = 16383;
    }
    pitch = Interpolate88(lut_vco_detune, pitch << 2);
  }

  osc.set_pitch(pitch + settings.pitch_transposition());

  if (trigger_flag) {
    osc.Strike();
    envelope.Trigger(ENV_SEGMENT_ATTACK);
    envelope2.Trigger(ENV_SEGMENT_ATTACK);
    trigger_flag = false;
  }
  
  // Enable hardsync only if level is a modulator destination when in envelope mode
  // or if both modulators are off
  if ((modulator1_mode == 1 && modulator1_destination & 2) ||
      (modulator2_mode == 1 && modulator2_destination & 2) ||
      (!modulator1_mode && !modulator2_mode)) {
    for (size_t i = 0; i < kAudioBlockSize; ++i) {
      sync_buffer[i] = sync_samples.ImmediateRead();
    }
  } else {
    // Disable hardsync when any of the shaping envelopes are not used for level.
    memset(sync_buffer, 0, sizeof(sync_buffer));
  }

  osc.Render(sync_buffer, render_buffer, kAudioBlockSize);

  // sum the envelope/LFO levels  
  uint32_t lad_value=0;
  if (modulator1_destination & 2) {
      lad_value = adv1;
  }
  if (modulator2_destination & 2) {
      lad_value += adv2;
  }
  if (lad_value > 65535) {
      lad_value = 65535;
  }
 
  // use AD envelope value as gain except in LFO mode, when it is used as 
  // negative gain from full volume.
  int32_t gain = 65535 ;
  if (modulator1_destination & 2 || modulator2_destination & 2) {
     if (modulator1_mode == 2 && modulator2_mode == 2) {
       gain = 65535 - ((lad_value >> 8) * ad_level_amount) ;
     } else {
        gain = lad_value ;
     }
  }
  
  // Copy to DAC buffer with sample rate and bit reduction applied.
  int16_t sample = 0;
  uint16_t bit_mask = bit_reduction_masks[settings.data().resolution];
  // sacrifice code size for performance by avoiding unnecessary computations
  if (settings.signature()) {
     for (size_t i = 0; i < kAudioBlockSize; ++i) {
       sample = render_buffer[i] & bit_mask;
       sample = ws.Transform(sample);
       sample = static_cast<int32_t>(sample) * gain >> 16;
       audio_samples.Overwrite(sample + 32768);
     }
  } else {
     for (size_t i = 0; i < kAudioBlockSize; ++i) {
       sample = render_buffer[i] & bit_mask;
       sample = static_cast<int32_t>(sample) * gain >> 16;
       audio_samples.Overwrite(sample + 32768);
     }
  }
  // debug_pin.Low();
}

int main(void) {
  Init();
  while (1) {
    while (audio_samples.writable() >= kAudioBlockSize && sync_samples.readable() >= kAudioBlockSize) {
      RenderBlock();
    }
    ui.DoEvents();
  }
}
