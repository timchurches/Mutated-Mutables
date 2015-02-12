// Copyright 2012 Olivier Gillet, 2015 Tim Churches
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
// Modifications: Tim Churches (tim.churches@gmail.com)
// Modifications may be determined by examining the differences between the last commit 
// by Olivier Gillet (pichenettes) and the HEAD commit at 
// https://github.com/timchurches/Mutated-Mutables/tree/master/braids 
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

#include <algorithm>

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
#include "braids/vco_jitter_source.h"
#include "braids/ui.h"

using namespace braids;
using namespace std;
using namespace stmlib;

const size_t kNumBlocks = 4;
const size_t kBlockSize = 24;

MacroOscillator osc;
Envelope envelope;  // first envelope/LFO 
Envelope envelope2; // second envelope/LFO 
Adc adc;
Dac dac;
DebugPin debug_pin;
GateInput gate_input;
InternalAdc internal_adc;
System sys;
VcoJitterSource jitter_source;
Ui ui;

size_t current_sample;
volatile size_t playback_block;
volatile size_t render_block;
int16_t audio_samples[kNumBlocks][kBlockSize];
uint8_t sync_samples[kNumBlocks][kBlockSize];

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
  ui.Poll();
}

void TIM1_UP_IRQHandler(void) {
  if (!(TIM1->SR & TIM_IT_Update)) {
    return;
  }
  TIM1->SR = (uint16_t)~TIM_IT_Update;
  
  dac.Write(audio_samples[playback_block][current_sample] + 32768);
  
  bool trigger_detected = gate_input.raised();
  sync_samples[playback_block][current_sample] = trigger_detected;
  trigger_detected_flag = trigger_detected_flag | trigger_detected;

  current_sample = current_sample + 1;
  if (current_sample >= kBlockSize) {
     current_sample = 0;
     playback_block = (playback_block + 1) % kNumBlocks;
  }  
  
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
  // debug_pin.Init();
  dac.Init();
  osc.Init();
  internal_adc.Init();
  
  for (size_t i = 0; i < kNumBlocks; ++i) {
    fill(&audio_samples[i][0], &audio_samples[i][kBlockSize], 0);
    fill(&sync_samples[i][0], &sync_samples[i][kBlockSize], 0);
    for (size_t j = 0; j < kBlockSize; ++j) {
       audio_samples[i][j] = j * 2730;
    }
  }
  playback_block = kNumBlocks / 2;
  render_block = 0;
  current_sample = 0;
     
  envelope.Init();
  envelope2.Init();
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
  static uint8_t metaseq_steps_index = 0;
  static int8_t metaseq_index = 0;
  static bool current_mseq_dir = true;
  static uint8_t mod1_sync_index = 0;
  static uint8_t mod2_sync_index = 0;
  // static uint8_t previous_metaseq_direction = 0;
  
  // debug_pin.High();

  uint8_t meta_mod = settings.GetValue(SETTING_META_MODULATION); // FMCV setting, in fact
  uint8_t modulator1_mode = settings.GetValue(SETTING_MOD1_MODE);
  uint8_t modulator2_mode = settings.GetValue(SETTING_MOD2_MODE);

  // use FM CV data for env params if envelopes or LFO modes are enabled
  // Note, we invert the parameter if in LFO mode, so higher voltages produce 
  // higher LFO frequencies
  uint32_t env_param = uint32_t (settings.GetValue(SETTING_MOD1_RATE));
  uint32_t env_a = 0;
  uint32_t env_d = 0;
  // add the external voltage to this.
  // scaling this by 32 seems about right for 0-5V modulation range.
  if (meta_mod == 2 || meta_mod == 3) {
	 env_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }

  // Clip at zero and 127
  if (env_param < 0) {
	 env_param = 0 ;
  } else if (env_param > 127) {
	 env_param = 127 ;
  } 
  // Invert if in LFO mode, so higher CVs create higher LFO frequency.
  if (modulator1_mode == 1 && settings.rate_inversion()) {
	 env_param = 127 - env_param ;
  }  
  // attack and decay parameters, default to FM voltage reading.
  env_a = env_param;
  env_d = env_param;
  // These are ratios of attack to decay, from A/D = 0.02 
  // through to A/D=0.9, then A=D, then D/A = 0.9 down to 0.02
  // as listed in ad_ratio_values in settings.cc
  uint8_t modulator1_ad_ratio = settings.GetValue(SETTING_MOD1_AD_RATIO);
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

  // now set the attack and decay parameters 
  // using the modified attack and decay values
  envelope.Update(env_a, env_d, 0, 0);  

  // Render envelope in LFO mode, or not
  // envelope 1
  uint8_t modulator1_attack_shape = settings.GetValue(SETTING_MOD1_ATTACK_SHAPE);
  uint8_t modulator1_decay_shape = settings.GetValue(SETTING_MOD1_DECAY_SHAPE);
  uint16_t ad_value = 0 ;
  if (modulator1_mode == 1) { 
	  // LFO mode
	  ad_value = envelope.Render(true, modulator1_attack_shape, modulator1_decay_shape);
  }
  else if (modulator1_mode > 1){
	  // envelope mode
	  ad_value = envelope.Render(false, modulator1_attack_shape, modulator1_decay_shape);
  }

  // TO-DO: instead of repeating code, use an array for env params and a loop!
  uint32_t env2_param = uint32_t (settings.GetValue(SETTING_MOD2_RATE));
  uint32_t env2_a = 0;
  uint32_t env2_d = 0;
  // add the external voltage to this.
  // scaling this by 32 seems about right for 0-5V modulation range.
  if (meta_mod == 2 || meta_mod == 4) {
	 env2_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }
  // Add cross-modulation
  int8_t mod1_mod2_depth = settings.GetValue(SETTING_MOD1_MOD2_DEPTH);
  if (mod1_mod2_depth) {
	env2_param +=  (ad_value * mod1_mod2_depth) >> 18;
  }
  // Clip at zero and 127
  if (env2_param < 0) { 
	 env2_param = 0 ;
  } else if (env2_param > 127) {
	 env2_param = 127 ;
  } 
  // Invert if in LFO mode, so higher CVs create higher LFO frequency.
  if (modulator2_mode == 1 && settings.rate_inversion()) { 
	 env2_param = 127 - env2_param ;
  }  
  env2_a = env2_param;
  env2_d = env2_param;
  // Repeat for envelope2
  uint8_t modulator2_ad_ratio = settings.GetValue(SETTING_MOD2_AD_RATIO);
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
  // now set the attack and decay parameters 
  // using the modified attack and decay values
  envelope2.Update(env2_a, env2_d, 0, 0);  

  // Render envelope in LFO mode, or not
  // envelope 2
  uint8_t modulator2_attack_shape = settings.GetValue(SETTING_MOD2_ATTACK_SHAPE);
  uint8_t modulator2_decay_shape = settings.GetValue(SETTING_MOD2_DECAY_SHAPE);
  uint16_t ad2_value = 0 ;
  if (modulator2_mode == 1) { 
	  // LFO mode
	  ad2_value = envelope2.Render(true, modulator2_attack_shape, modulator2_decay_shape);
  }
  else if (modulator2_mode > 1) {
	  // envelope mode
	  ad2_value = envelope2.Render(false, modulator2_attack_shape, modulator2_decay_shape);
  }

  // modulate timbre
  int32_t parameter_1 = adc.channel(0) << 3; 
  if (modulator1_mode == 2) {
	 parameter_1 -= (ad_value * settings.mod1_timbre_depth()) >> 9;
  } else {
	 parameter_1 += (ad_value * settings.mod1_timbre_depth()) >> 9;
  }  
  if (settings.mod1_mod2_timbre_depth()) {   
	 int32_t timbre_delta = (ad2_value * settings.mod2_timbre_depth()) >> 9;
	 timbre_delta = (timbre_delta * ad_value) >> 16;
	 if (modulator2_mode == 2) {  
		parameter_1 -= timbre_delta;
	 } else {
		parameter_1 += timbre_delta;
	 }
  } else {
	 if (modulator2_mode == 2) {  
		parameter_1 -= (ad2_value * settings.mod2_timbre_depth()) >> 9;
	 } else {
		parameter_1 += (ad2_value * settings.mod2_timbre_depth()) >> 9;
	 }
  }
  // clip
  if (parameter_1 > 32767) {
	parameter_1 = 32767;
  } else if (parameter_1 < 0) {
	parameter_1 = 0;
  }

  // modulate colour
  int32_t parameter_2 = adc.channel(1) << 3; 
  if (modulator1_mode == 2) {
	 parameter_2 -= (ad_value * settings.mod1_color_depth()) >> 9;
  } else {
	 parameter_2 += (ad_value * settings.mod1_color_depth()) >> 9;
  }
  if (settings.mod1_mod2_color_depth()) {   
	 int32_t color_delta = (ad2_value * settings.mod2_color_depth()) >> 9;
	 color_delta = (color_delta * ad_value) >> 16;
	 if (modulator2_mode == 2) {  
		parameter_2 -= color_delta;
	 } else {
		parameter_2 += color_delta;
	 }
  } else {
	 if (modulator2_mode == 2) {  
		parameter_2 -= (ad2_value * settings.mod2_color_depth()) >> 9;
	 } else {
		parameter_2 += (ad2_value * settings.mod2_color_depth()) >> 9;
	 }
  }
  // clip
  if (parameter_2 > 32767) {
	parameter_2 = 32767;
  } else if (parameter_2 < 0) {
	parameter_2 = 0;
  }
  
  // set the timbre and color parameters on the oscillator
  osc.set_parameters(uint16_t(parameter_1), uint16_t(parameter_2));

  // meta_modulation no longer a boolean  
  // meta-sequencer over-rides FMCV=META and the WAVE setting
  uint8_t metaseq_length = settings.GetValue(SETTING_METASEQ);
  if (!metaseq_length) {
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
  } 
  
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
  
    // add vibrato from modulators 1 and 2 before or after quantisation
  uint8_t mod1_vibrato_depth = settings.GetValue(SETTING_MOD1_VIBRATO_DEPTH); // 0 to 127
  uint8_t mod2_vibrato_depth = settings.GetValue(SETTING_MOD2_VIBRATO_DEPTH); // 0 to 127
  bool mod1_mod2_vibrato_depth = settings.mod1_mod2_vibrato_depth();
  bool quantize_vibrato = settings.quantize_vibrato();

  if (quantize_vibrato) {
     // vibrato should be bipolar
     if (mod1_vibrato_depth) {
        if (modulator1_mode == 2) {
           pitch -= ((ad_value - 32767) * mod1_vibrato_depth) >> 11 ; 
        } else {  
           pitch += ((ad_value - 32767) * mod1_vibrato_depth) >> 11 ; 
        }    
     }
     // mod1 envelope mediates the degree of vibrato from mod2, or not.
     if (mod1_mod2_vibrato_depth) {
        if (mod2_vibrato_depth) {
           int32_t pitch_delta = ((ad2_value - 32767) * mod2_vibrato_depth) >> 11;
           pitch_delta = (pitch_delta * ad_value) >> 16;
           if (modulator2_mode == 2) {
              pitch -= pitch_delta;  
           } else {
              pitch += pitch_delta; 
           }   
        }
     } else {
        if (mod2_vibrato_depth) {
           if (modulator2_mode == 2) {
              pitch -= ((ad2_value - 32767) * mod2_vibrato_depth) >> 11 ;  
           } else {
              pitch += ((ad2_value - 32767) * mod2_vibrato_depth) >> 11 ; 
           }        
        }
     }
  }
  
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

  // Or add vibrato here
  if (!quantize_vibrato) {
     if (mod1_vibrato_depth) {
        if (modulator1_mode == 2) {
           pitch -= ((ad_value - 32767) * mod1_vibrato_depth) >> 11 ; 
        } else {  
           pitch += ((ad_value - 32767) * mod1_vibrato_depth) >> 11 ; 
        }    
     }
     // mod1 envelope mediate the degree of vibrato from mod2, or not.
     if (mod1_mod2_vibrato_depth) {
        if (mod2_vibrato_depth) {
           int32_t pitch_delta = ((ad2_value - 32767) * mod2_vibrato_depth) >> 11;
           pitch_delta = (pitch_delta * ad_value) >> 16;
           if (modulator2_mode == 2) {
              pitch -= pitch_delta;  
           } else {
              pitch += pitch_delta; 
           }   
        }
     } else {
        if (mod2_vibrato_depth) {
           if (modulator2_mode == 2) {
              pitch -= ((ad2_value - 32767) * mod2_vibrato_depth) >> 11 ;  
           } else {
              pitch += ((ad2_value - 32767) * mod2_vibrato_depth) >> 11 ; 
           }        
        }
     }
  }

  // jitter depth now settable and voltage controllable.
  // TO-DO jitter still causes pitch to sharpen slightly - why?
  int32_t vco_drift = settings.vco_drift();
  if (meta_mod == 6 || meta_mod == 9 || meta_mod == 10 || meta_mod == 12) {
     vco_drift += settings.adc_to_fm(adc.channel(3)) >> 6;
  } 
  if (vco_drift) {
     if (vco_drift < 0) {
	    vco_drift = 0 ;
     } else if (vco_drift > 127) {
        vco_drift = 127;
     }
    // now apply the jitter
    pitch +=  (jitter_source.Render(adc.channel(1) << 3) >> 8) * vco_drift;
  }

  // clip the pitch to prevent bad things from happening.
  if (pitch > 32767) {
    pitch = 32767;
  } else if (pitch < 0) {
    pitch = 0;
  }
  
  osc.set_pitch(pitch + settings.pitch_transposition());

  if (trigger_flag) {
    osc.Strike();
    // reset internal modulator phase if mod1_sync or mod2_sync > 0
    // and if a trigger counter for each = the setting of mod1_sync
    // or mod2_sync (defaults to 1 thus every trigger).
    if (settings.GetValue(SETTING_MOD1_SYNC)) {
       mod1_sync_index += 1;
       if (mod1_sync_index >= settings.GetValue(SETTING_MOD1_SYNC)) {
          envelope.Trigger(ENV_SEGMENT_ATTACK);
          mod1_sync_index = 0 ;
       }
    }
    if (settings.GetValue(SETTING_MOD2_SYNC)) {
       mod2_sync_index += 1;
       if (mod2_sync_index >= settings.GetValue(SETTING_MOD2_SYNC)) {
          envelope2.Trigger(ENV_SEGMENT_ATTACK);
          mod2_sync_index = 0 ;
       }
    }
    // meta-sequencer
	if (metaseq_length) {
		 MacroOscillatorShape metaseq_shapes[8] = { settings.metaseq_shape1(),
						   settings.metaseq_shape2(), settings.metaseq_shape3(),
						   settings.metaseq_shape4(), settings.metaseq_shape5(),
						   settings.metaseq_shape6(), settings.metaseq_shape7(),
						   settings.metaseq_shape8() };                   
		 uint8_t metaseq_step_lengths[8] = { 
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH1),
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH2),   
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH3),
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH4),
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH5),
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH6),
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH7),
						   settings.GetValue(SETTING_METASEQ_STEP_LENGTH8) };
	     metaseq_steps_index += 1;
		 uint8_t metaseq_direction = settings.GetValue(SETTING_METASEQ_DIRECTION);
		 // if (metaseq_direction != previous_metaseq_direction) {
		 //    metaseq_index = 0;   
		 //    metaseq_steps_index = 0;
		 // }
		 if (metaseq_steps_index == (metaseq_step_lengths[metaseq_index])) { 
			  metaseq_steps_index = 0;
			  if (metaseq_direction == 0) {
				 // looping
				 metaseq_index += 1;
				 if (metaseq_index > (metaseq_length - 1)) { 
					metaseq_index = 0;
				 }
			  } else if (metaseq_direction == 1) {
				 // swing
				 if (current_mseq_dir) {
					// ascending
					metaseq_index += 1;
					if (metaseq_index >= (metaseq_length - 1)) {
					   metaseq_index = metaseq_length - 1; 
					   current_mseq_dir = !current_mseq_dir;
					}
				 } else {
					// descending
					metaseq_index -= 1;
					if (metaseq_index == 0) { 
					   current_mseq_dir = !current_mseq_dir;
					}
				}             
			  } else if (metaseq_direction == 2) {
				 // random
				 metaseq_index = uint8_t(Random::GetWord() >> 29);
			  }
		 }
		 MacroOscillatorShape metaseq_current_shape = metaseq_shapes[metaseq_index];
		 osc.set_shape(metaseq_current_shape);
		 ui.set_meta_shape(metaseq_current_shape);
		 // previous_metaseq_direction = metaseq_direction;
	}
    ui.StepMarquee(); // retained because this is what causes the CV tester to blink on each trigger
    trigger_flag = false;
  }

  uint8_t* sync_buffer = sync_samples[render_block];
  int16_t* render_buffer = audio_samples[render_block];
  if (!settings.osc_sync()) {
    // Disable hardsync when oscillator sync disabled.
    memset(sync_buffer, 0, kBlockSize);
   }

  osc.Render(sync_buffer, render_buffer, kBlockSize);

  // gain is a weighted sum of the envelope/LFO levels  
  uint32_t mod1_level_depth = uint32_t(settings.mod1_level_depth());
  uint32_t mod2_level_depth = uint32_t(settings.mod2_level_depth());
  int32_t gain = settings.initial_gain(); 
  // add external CV if FMCV used for level
  if (meta_mod == 5) {
     gain += settings.adc_to_fm(adc.channel(3)) << 4; // was 3 
  } 
  // Gain mod by modulator 1
  if (modulator1_mode  && modulator1_mode < 3) {
     // subtract from full gain if LFO-only modes (mode==1) or Env- modes (mode==2)
     gain -= (ad_value * mod1_level_depth) >> 8;
  } else if (modulator1_mode == 3) {
     gain += (ad_value * mod1_level_depth) >> 8;
  }
  // Gain mod by modulator 2
  if (modulator2_mode  && modulator2_mode < 3) {
     // subtract from full gain if LFO-only modes (mode==1) or Env- modes (mode==2)
     gain -= (ad2_value * mod2_level_depth) >> 8;
  } else if (modulator2_mode == 3) {
     gain += (ad2_value * mod2_level_depth) >> 8;
  }
  // clip the gain  
  if (gain > 65535) {
      gain = 65535;
  }
  else if (gain < 0) {
      gain = 0;
  }

  // Voltage control of bit crushing
  uint8_t bits_value = settings.resolution();
  if (meta_mod == 7 || meta_mod == 9 || meta_mod >= 11 ) {
     bits_value -= settings.adc_to_fm(adc.channel(3)) >> 9;
     if (bits_value < 0) {
	    bits_value = 0 ;
     } else if (bits_value > 6) {
        bits_value = 6;
     }
  }

  // Voltage control of sample rate decimation
  uint8_t sample_rate_value = settings.data().sample_rate;
  if (meta_mod == 8 || meta_mod >= 10 ) {
     sample_rate_value -= settings.adc_to_fm(adc.channel(3)) >> 9;
     if (sample_rate_value < 0) {
	    sample_rate_value = 0 ;
     } else if (sample_rate_value > 6) {
        sample_rate_value = 6;
     }
  }
     
  // Copy to DAC buffer with sample rate and bit reduction applied.
  int16_t sample = 0;
  size_t decimation_factor = decimation_factors[sample_rate_value];  
  uint16_t bit_mask = bit_reduction_masks[bits_value];
  for (size_t i = 0; i < kBlockSize; ++i) {
    if ((i % decimation_factor) == 0) {
       sample = render_buffer[i] & bit_mask;
    }
    render_buffer[i] = static_cast<int32_t>(sample) * gain >> 16;
  }
  render_block = (render_block + 1) % kNumBlocks;
  // debug_pin.Low();
}

int main(void) {
  Init();
  while (1) {
    while (render_block != playback_block) {
      RenderBlock();
    }
    ui.DoEvents();
  }
}
