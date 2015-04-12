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
static int32_t sh_pitch;

// Templated function to do parameter clipping
template <typename ParamType> 
static ParamType ParamClip(ParamType param, ParamType min_param, ParamType max_param) {
    if (param < min_param) {
	   return min_param ;
    } else if (param > max_param) {
	   return max_param ;
    } else {
       return param;
    }
}

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
  
  sh_pitch = 69 << 7;
     
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

// table of log2 values for harmonic series quantisation, generated by the
// following R code: round(log2(1:37)*2048)
const uint16_t log2_table[] = { 0, 2048, 3246, 4096, 4755, 5294, 5749, 6144, 6492, 6803,
                                7085, 7342, 7579, 7797, 8001, 8192, 8371, 8540, 8700,
                                8851, 8995, 9133, 9264, 9390, 9511, 9627, 9738, 9845,
                                9949, 10049, 10146, 10240, 10331, 10419, 10505, 10588,
                                10669, };

// following table adapted from Mutable Instruments MIDIpal source code, resources.py
const uint8_t turing_scales[] = 
    { 0, 2, 4, 5, 7,  9,  11, 12,              // Ionian = 7
      0, 2, 3, 5, 7,  9,  10, 12,              // Dorian = 7
      0, 1, 3, 5, 7,  8,  10, 12,              // Phrygian = 7
      0, 2, 4, 6, 7,  9,  11, 12,              // Lydian = 7
      0, 2, 4, 5, 7,  9,  10, 12,              // Mixolydian = 7
      0, 2, 3, 5, 7,  8,  10, 12,              // Aeolian = 7
      0, 1, 3, 5, 6,  8,  10, 12,              // Locrian = 7
      0, 3, 4, 7, 9,  10, 12, 15,              // Blues major = 6
      0, 3, 5, 6, 7,  10, 12, 15,              // Blues minor = 6
      0, 2, 4, 7, 9,  12, 14, 16,              // Pentatonic major = 5
      0, 3, 5, 7, 10, 12, 15, 17,              // Pentatonic minor = 5
      0, 1, 4, 5, 7,  8,  11, 12,              // Bhairav = 7
      0, 1, 4, 6, 7,  8,  11, 12,              // Shri = 7
      0, 1, 3, 5, 7,  10, 11, 12,              // Rupavati = 7
      0, 1, 3, 6, 7,  8,  11, 12,              // Todi = 7
      0, 2, 4, 5, 9,  10, 11, 12,              // Rageshri = 7
      0, 2, 3, 5, 7,  9,  10, 12,              // Kaafi = 7
      0, 2, 5, 7, 9,  12, 14, 17,              // Megh = 5
      0, 3, 5, 8, 10, 12, 15, 17,              // Malkauns = 5
      0, 3, 4, 6, 8,  10, 12, 15,              // Deepak = 6
      0, 1, 3, 4, 5,  7,  8,  10,              // Folk = 8
      0, 1, 5, 7, 8,  12, 13, 17,              // Japanese = 5
      0, 1, 3, 7, 8,  12, 13, 15,              // Gamelan = 5
      0, 2, 4, 6, 8,  10, 12, 14, };           // Whole tone = 6

const uint8_t turing_divisors[] = { 7, // Ionian = 7
                                  7, // Dorian = 7
                                  7, // Phrygian = 7
                                  7, // Lydian = 7
                                  7, // Mixolydian = 7
                                  7, // Aeolian = 7
                                  7, // Locrian = 7
                                  6, // Blues major = 6
                                  6, // Blues minor = 6
                                  5, // Pentatonic major = 5
                                  5, // Pentatonic minor = 5 
                                  7, // Bhairav = 7
                                  7, // Shri = 7
                                  7, // Rupavati = 7
                                  7, // Todi = 7
                                  7, // Rageshri = 7
                                  7, // Kaafi = 7
                                  5, // Megh = 5
                                  5, // Malkauns = 5
                                  6, // Deepak = 6
                                  8, // Folk = 8
                                  5, // Japanese = 5
                                  5, // Gamelan = 5
                                  6, // Whole tone = 6
};

// following table adapted from Mutable Instruments MIDIpal source code, resources.cc
const uint8_t quant_scales[] = {
// Ionian
       0,      0,      2,      2,      4,      5,      5,      7,
       7,      9,      9,     11,
//Dorian
       0,      0,      2,      3,      3,      5,      5,      7,
       7,      9,     10,     10,
//Phrygian
       0,      1,      1,      3,      3,      5,      5,      7,
       8,      8,     10,     10,
// Lydian
       0,      0,      2,      2,      4,      4,      6,      7,
       7,      9,      9,     11,
// Myxolydian
       0,      0,      2,      2,      4,      5,      5,      7,
       7,      9,     10,     10,
// Aeolian
       0,      0,      2,      3,      3,      5,      5,      7,
       8,      8,     10,     10,
// Locrian
       0,      1,      1,      3,      3,      5,      6,      6,
       8,      8,     10,     10,
// Blues major
       0,      0,      3,      3,      4,      4,      7,      7,
       7,      9,     10,     10,
// Blues minor
       0,      0,      3,      3,      3,      5,      6,      7,
       7,     10,     10,     10,
// Pentatonic major
       0,      0,      2,      2,      4,      4,      7,      7,
       7,      9,      9,      9,
// Pentatonic minor
       0,      0,      3,      3,      3,      5,      5,      7,
       7,     10,     10,     10,
// Bhairav
       0,      1,      1,      4,      4,      5,      5,      7,
       8,      8,     11,     11,
// Shri
       0,      1,      1,      4,      4,      4,      6,      7,
       8,      8,     11,     11,
// Rupavati
       0,      1,      1,      3,      3,      5,      5,      7,
       7,     10,     10,     11,
// Todi
       0,      1,      1,      3,      3,      6,      6,      7,
       8,      8,     11,     11,
// Rageshri
       0,      0,      2,      2,      4,      5,      5,      5,
       9,      9,     10,     11,
// Kaafi
       0,      0,      2,      2,      3,      3,      5,      5,
       7,      7,      9,     10,

// Megh
       0,      0,      2,      2,      5,      5,      5,      7,
       7,      9,      9,      9,
// Malkauns
       0,      0,      3,      3,      3,      5,      5,      8,
       8,      8,     10,     10,
// Deepak
       0,      0,      3,      3,      4,      4,      6,      6,
       8,      8,     10,     10,
// Folk
       0,      1,      1,      3,      4,      5,      5,      7,
       8,      8,     10,     10,
// Japanese
       0,      1,      1,      1,      5,      5,      5,      7,
       8,      8,      8,      8,
// Gamelan
       0,      1,      1,      3,      3,      3,      7,      7,
       8,      8,      8,      8,
// Whole tone
       0,      0,      2,      2,      4,      4,      6,      6,
       8,      8,     10,     10,
};
                                
void RenderBlock() {
  static uint16_t previous_pitch_adc_code = 0;
  static uint16_t previous_fm_adc_code = 0;
  static int32_t previous_pitch = 0;
  static int32_t metaseq_pitch_delta = 0;
  static int32_t previous_shape = 0;
  static uint8_t metaseq_div_counter = 0;
  static uint8_t metaseq_steps_index = 0;
  static uint8_t prev_metaseq_direction = 0;
  static int8_t metaseq_index = 0;
  static bool current_mseq_dir = true;
  static uint8_t mod1_sync_index = 0;
  static uint8_t mod2_sync_index = 0;
  static uint8_t metaseq_parameter = 0;
  static uint8_t turing_div_counter = 0;
  static uint8_t turing_init_counter = 0;
  static uint32_t turing_shift_register = 0;
  static int32_t turing_pitch_delta = 0;
  
  // debug_pin.High();

  uint8_t meta_mod = settings.GetValue(SETTING_META_MODULATION); // FMCV setting, in fact
  uint8_t modulator1_mode = settings.GetValue(SETTING_MOD1_MODE);
  uint8_t modulator2_mode = settings.GetValue(SETTING_MOD2_MODE);

  // use FM CV data for env params if envelopes or LFO modes are enabled
  // Note, we invert the parameter if in LFO mode, so higher voltages produce 
  // higher LFO frequencies
  uint32_t env_a_param = uint32_t (settings.GetValue(SETTING_MOD1_RATE));
  uint32_t env_d_param = env_a_param;
  uint32_t env_a = 0;
  uint32_t env_d = 0;
  // add the external voltage to this.
  // scaling this by 32 seems about right for 0-5V modulation range.
  if (meta_mod == 2 || meta_mod == 3) {
	 env_a_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }
  if (meta_mod == 2 || meta_mod == 3 || meta_mod == 5 || meta_mod == 6) {
	 env_d_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }

  // Clip at zero and 127
//   if (env_a_param < 0) {
// 	 env_a_param = 0 ;
//   } else if (env_a_param > 127) {
// 	 env_a_param = 127 ;
//   } 
  env_a_param = ParamClip(env_a_param, 0ul, 127ul);
  
//   if (env_d_param < 0) {
// 	 env_d_param = 0 ;
//   } else if (env_d_param > 127) {
// 	 env_d_param = 127 ;
//   } 
  env_d_param = ParamClip(env_d_param, 0ul, 127ul);

  // Invert if in LFO mode, so higher CVs create higher LFO frequency.
  if (modulator1_mode == 1 && settings.rate_inversion()) {
	 env_a_param = 127 - env_a_param ;
	 env_d_param = 127 - env_d_param ;
  }  

  // attack and decay parameters, default to FM voltage reading.
  // These are ratios of attack to decay, from A/D = 0 to 127
  env_a = ((1 + (settings.GetValue(SETTING_MOD1_AD_RATIO))) * env_a_param) >> 6;  
  env_d = ((128 - (settings.GetValue(SETTING_MOD1_AD_RATIO))) * env_d_param) >> 6;   

  // Clip at zero and 127
//   if (env_a < 0) {
// 	 env_a = 0 ;
//   } else if (env_a > 127) {
// 	 env_a = 127 ;
//   } 
  env_a = ParamClip(env_a, 0ul, 127ul);

//   if (env_d < 0) {
// 	 env_d = 0 ;
//   } else if (env_d > 127) {
// 	 env_d = 127 ;
//   } 
  env_d = ParamClip(env_d, 0ul, 127ul);

  // Render envelope in LFO mode, or not
  // envelope 1
  bool LFO_mode = false;
  if (modulator1_mode == 1) { 
	  // LFO mode
	  LFO_mode = true;
  } else if (modulator1_mode > 1) {
	  // envelope mode
	  LFO_mode = false;  
  }	  
  // now set the attack and decay parameters 
  // using the modified attack and decay values
  envelope.Update(env_a, env_d, 0, 0, LFO_mode, settings.GetValue(SETTING_MOD1_ATTACK_SHAPE), settings.GetValue(SETTING_MOD1_DECAY_SHAPE));  
  // Render the envelope
  uint16_t ad_value = envelope.Render() ;


  // TO-DO: instead of repeating code, use an array for env params and a loop!
  // Note: tried in branch envelope-tidy-up, but resulted in bigger compiled size
  uint32_t env2_a_param = uint32_t (settings.GetValue(SETTING_MOD2_RATE));
  uint32_t env2_d_param = env2_a_param;
  uint32_t env2_a = 0;
  uint32_t env2_d = 0;
  // add the external voltage to this.
  // scaling this by 32 seems about right for 0-5V modulation range.
  if (meta_mod == 2 || meta_mod == 4) {
	 env2_a_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }
  if (meta_mod == 2 || meta_mod == 4 || meta_mod == 5 || meta_mod == 7) {
	 env2_d_param += settings.adc_to_fm(adc.channel(3)) >> 5;
  }
  // Add cross-modulation
  int8_t mod1_mod2_depth = settings.GetValue(SETTING_MOD1_MOD2_DEPTH);
  if (mod1_mod2_depth) {
	env2_a_param +=  (ad_value * mod1_mod2_depth) >> 18;
	env2_a_param +=  (ad_value * mod1_mod2_depth) >> 18;
  }
  // Clip at zero and 127
//   if (env2_a_param < 0) { 
// 	 env2_a_param = 0 ;
//   } else if (env2_a_param > 127) {
// 	 env2_a_param = 127 ;
//   } 
  env2_a_param = ParamClip(env2_a_param, 0ul, 127ul);
//   if (env2_d_param < 0) { 
// 	 env2_d_param = 0 ;
//   } else if (env2_d_param > 127) {
// 	 env2_d_param = 127 ;
//   } 
  env2_d_param = ParamClip(env2_d_param, 0ul, 127ul);
  
  if (modulator2_mode == 1 && settings.rate_inversion()) { 
	 env2_a_param = 127 - env2_a_param ;
	 env2_d_param = 127 - env2_d_param ;
  }  

  // These are ratios of attack to decay, from A/D = 0 to 127
  env2_a = ((1 + settings.GetValue(SETTING_MOD2_AD_RATIO)) * env2_a_param) >> 6; 
  env2_d = ((128 - settings.GetValue(SETTING_MOD2_AD_RATIO)) * env2_d_param) >> 6; 

  // Clip at zero and 127
//   if (env2_a < 0) {
// 	 env2_a = 0 ;
//   } else if (env2_a > 127) {
// 	 env2_a = 127 ;
//   } 
  env2_a = ParamClip(env2_a, 0ul, 127ul);
//   if (env2_d < 0) {
// 	 env2_d = 0 ;
//   } else if (env2_d > 127) {
// 	 env2_d = 127 ;
//   } 
  env2_d = ParamClip(env2_d, 0ul, 127ul);
 
  // Render envelope in LFO mode, or not
  // envelope 2
  if (modulator2_mode == 1) { 
	  // LFO mode
	  LFO_mode = true;
  } else if (modulator2_mode > 1) {
	  // envelope mode
	  LFO_mode = false;  
  }	  
  // now set the attack and decay parameters 
  // using the modified attack and decay values
  envelope2.Update(env2_a, env2_d, 0, 0, LFO_mode, settings.GetValue(SETTING_MOD2_ATTACK_SHAPE), settings.GetValue(SETTING_MOD2_DECAY_SHAPE));  
  // Render the envelope
  uint16_t ad2_value = envelope2.Render() ;

  // meta-sequencer
  uint8_t metaseq_length = settings.GetValue(SETTING_METASEQ);
  if (trigger_flag && metaseq_length) {
     ++metaseq_div_counter;
     if (metaseq_div_counter >= settings.GetValue(SETTING_METASEQ_CLOCK_DIV)) {
        metaseq_div_counter = 0;
	    uint8_t metaseq_direction = settings.GetValue(SETTING_METASEQ_DIRECTION);
        if (metaseq_direction != prev_metaseq_direction) {
           prev_metaseq_direction = metaseq_direction;
           metaseq_steps_index = 0;
           metaseq_index = 0;
           current_mseq_dir = true;
        }
	    ++metaseq_steps_index;
	    if (metaseq_steps_index >= (settings.metaseq_step_length(metaseq_index))) { 
	       metaseq_steps_index = 0;
		   if (metaseq_direction == 0) {
		      // looping
		      ++metaseq_index;
		      if (metaseq_index > metaseq_length) { 
		         metaseq_index = 0;
		      }
		   } else if (metaseq_direction == 1) {
		      // swing
		      if (current_mseq_dir) {
		         // ascending
			     ++metaseq_index;
			     if (metaseq_index >= metaseq_length) {
			        metaseq_index = metaseq_length; 
				    current_mseq_dir = !current_mseq_dir;
			     }
		      } else {
			     // descending
			     --metaseq_index;
			     if (metaseq_index == 0) { 
			        current_mseq_dir = !current_mseq_dir;
			      }
		       }             
		   } else if (metaseq_direction == 2) {
		     // random
		     metaseq_index = (uint8_t(Random::GetWord() >> 29) * (metaseq_length + 1)) >> 3;
		   }
        }
	    MacroOscillatorShape metaseq_current_shape = settings.metaseq_shape(metaseq_index);
	    osc.set_shape(metaseq_current_shape);
	    ui.set_meta_shape(metaseq_current_shape);
	    metaseq_pitch_delta = settings.metaseq_note(metaseq_index) << 7;
        metaseq_parameter = settings.metaseq_parameter(metaseq_index) ;
     }
  } // end meta-sequencer
  
  // Turing machine
  int16_t turing_length = static_cast<int16_t>(settings.GetValue(SETTING_TURING_LENGTH));
  // Add to the Turing shift register length if FMCV=TRNG
  if (meta_mod == 10) {
     // add the FM CV amount
	 turing_length += settings.adc_to_fm(adc.channel(3)) >> 7;
     // Clip at zero and 32
//      if (turing_length < 0) { 
//         turing_length = 0 ;
//      } else if (turing_length > 32) {
//         turing_length = 32 ;
//      } 
        turing_length = ParamClip(turing_length, static_cast<int16_t>(0), static_cast<int16_t>(32));
  }
  if (trigger_flag && turing_length) {
     ++turing_div_counter;
     if (turing_div_counter >= settings.GetValue(SETTING_TURING_CLOCK_DIV)) {
        turing_div_counter = 0;
        // initialise the shift register if required
        if (!turing_shift_register) {
           turing_shift_register = Random::GetWord();
        }
        // re-initialise the shift register with random data if required
        if (settings.GetValue(SETTING_TURING_INIT)) {
           ++turing_init_counter;
           if (turing_init_counter >= settings.GetValue(SETTING_TURING_INIT)) {
              turing_init_counter = 0;
              turing_shift_register = Random::GetWord();
           }
        }
        // read the LSB
        bool turing_lsb = turing_shift_register & static_cast<uint32_t>(1);
        bool turing_remainder_lsb = false;
        // read the LSB in the remainder of the shift register
        if (turing_length < 32) {
           turing_remainder_lsb = turing_shift_register & (static_cast<uint32_t>(1) << turing_length);
        }
        // rotate the shift register
        turing_shift_register = turing_shift_register >> 1;
        // add back the LSB into the MSB postion
        if (turing_lsb) {
           turing_shift_register |= (static_cast<uint32_t>(1) << (turing_length - 1));
        } else {
           turing_shift_register &= (~(static_cast<uint32_t>(1) << (turing_length - 1)));
        }
        // add back the LSB to the remainder of the shift register
        if (turing_length < 32) {
           if (turing_remainder_lsb) {
              turing_shift_register |= (static_cast<uint32_t>(1) << 31);
           } else {
              turing_shift_register &= (~(static_cast<uint32_t>(1) << 31));
           }
        }
        // decide whether to flip the LSB
        int16_t turing_prob = settings.GetValue(SETTING_TURING_PROB);
        if (meta_mod == 11) {
           // add the FM CV amount
	       turing_prob += settings.adc_to_fm(adc.channel(3)) >> 5;
        }
        // Clip at zero and 127
//         if (turing_prob < 0) { 
// 	       turing_prob = 0 ;
//         } else if (turing_prob > 127) {
// 	       turing_prob = 127 ;
//         } 
        turing_prob = ParamClip(turing_prob, static_cast<int16_t>(0), static_cast<int16_t>(127));

        if ((static_cast<uint8_t>(Random::GetWord() >> 23) < turing_prob) || turing_prob == 127) {
           // bit-flip the LSB, bit-shift was 25 but try making is 4 times less sensitive
           // yes, leave at 23 but force bit-flip if turing_prob is 127.
           turing_shift_register = turing_shift_register ^ static_cast<uint32_t>(1) ;
        }
        // read the window and calculate pitch increment
        int16_t turing_window = settings.GetValue(SETTING_TURING_WINDOW);
        if (meta_mod == 12) {
           // add the FM CV amount, offset by 2
	       turing_window += (settings.adc_to_fm(adc.channel(3)) >> 7) + 2;
        }
        // Clip at zero and 36
//         if (turing_window < 0) { 
// 	       turing_window = 0 ;
//         } else if (turing_window > 36) {
// 	       turing_window = 36 ;
//         }         
        turing_window = ParamClip(turing_window, static_cast<int16_t>(0), static_cast<int16_t>(36));

        uint32_t turing_byte = turing_shift_register & static_cast<uint32_t>(0xFF);
        uint8_t turing_value = (turing_byte * static_cast<uint8_t>(turing_window)) >> 8;
        // convert into a pitch increment
        if (settings.GetValue(SETTING_MUSICAL_SCALE) == 0) {
           turing_pitch_delta = turing_value << 7 ;
        } else if (settings.GetValue(SETTING_MUSICAL_SCALE) < 25) {
           uint8_t turing_whole_octaves = turing_value / turing_divisors[(settings.GetValue(SETTING_MUSICAL_SCALE) - 1)] ;
           uint8_t turing_remainder_semitones = turing_value - (turing_whole_octaves * turing_divisors[(settings.GetValue(SETTING_MUSICAL_SCALE) - 1)]);          
           turing_pitch_delta = ((turing_whole_octaves * 12) + turing_scales[((settings.GetValue(SETTING_MUSICAL_SCALE) - 1) << 3) + turing_remainder_semitones]) << 7 ;
        } else if (settings.GetValue(SETTING_MUSICAL_SCALE) == 25) {
           // Harmonic series
           turing_pitch_delta = (1536 * log2_table[turing_value]) >> 11;
        }
     }
  } // end Turing machine

  // modulate timbre
  int32_t parameter_1 = adc.channel(0) << 3; 
  if (modulator1_mode == 2) {
	 parameter_1 -= (ad_value * settings.mod1_timbre_depth()) >> 9;
  } else {
	 parameter_1 += (ad_value * settings.mod1_timbre_depth()) >> 9;
  }  
  if (modulator2_mode == 2) {  
     parameter_1 -= (ad2_value * settings.mod2_timbre_depth()) >> 9;
  } else {
     parameter_1 += (ad2_value * settings.mod2_timbre_depth()) >> 9;
  }
  // scale the gain by the meta-sequencer parameter if applicable
  if (metaseq_length && (settings.GetValue(SETTING_METASEQ_PARAMETER_DEST) & 1)) {
     parameter_1 = (parameter_1 * metaseq_parameter) >> 7;
  }
  // clip
//   if (parameter_1 > 32767) {
// 	parameter_1 = 32767;
//   } else if (parameter_1 < 0) {
// 	parameter_1 = 0;
//   }
  parameter_1 = ParamClip(parameter_1, static_cast<int32_t>(0), static_cast<int32_t>(32767));

  // modulate colour
  int32_t parameter_2 = adc.channel(1) << 3; 
  if (modulator1_mode == 2) {
	 parameter_2 -= (ad_value * settings.mod1_color_depth()) >> 9;
  } else {
	 parameter_2 += (ad_value * settings.mod1_color_depth()) >> 9;
  }
  if (modulator2_mode == 2) {  
	 parameter_2 -= (ad2_value * settings.mod2_color_depth()) >> 9;
  } else {
	 parameter_2 += (ad2_value * settings.mod2_color_depth()) >> 9;
  }
  // scale the gain by the meta-sequencer parameter if applicable
  if (metaseq_length && (settings.GetValue(SETTING_METASEQ_PARAMETER_DEST) & 2)) {
     parameter_2 = (parameter_2 * metaseq_parameter) >> 7;
  }
  // clip
//   if (parameter_2 > 32767) {
// 	parameter_2 = 32767;
//   } else if (parameter_2 < 0) {
// 	parameter_2 = 0;
//   }
  parameter_2 = ParamClip(parameter_2, static_cast<int32_t>(0), static_cast<int32_t>(32767));
  
  // set the timbre and color parameters on the oscillator
  osc.set_parameters(uint16_t(parameter_1), uint16_t(parameter_2));

  // meta_modulation no longer a boolean  
  // meta-sequencer over-rides FMCV=META and the WAVE setting
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

  // Sample and hold pitch if enabled
  if (settings.pitch_sample_hold()) {
     if (trigger_flag) {
        sh_pitch = pitch;
     }
     pitch = sh_pitch; 
  }
  
  // add vibrato from modulators 1 and 2 before or after quantisation
  uint8_t mod1_vibrato_depth = settings.GetValue(SETTING_MOD1_VIBRATO_DEPTH); // 0 to 127
  uint8_t mod2_vibrato_depth = settings.GetValue(SETTING_MOD2_VIBRATO_DEPTH); // 0 to 127
  bool mod1_mod2_vibrato_depth = settings.mod1_mod2_vibrato_depth();
  bool quantize_vibrato = settings.quantize_vibrato();
  int32_t pitch_delta1 = 0 ;
  int32_t pitch_delta2 = 0 ;

  // calculate vibrato amount, vibrato should be bipolar
  if (mod1_vibrato_depth) {
     pitch_delta1 = ((ad_value - 32767) * mod1_vibrato_depth) >> 11 ; 
  }
    
  // mod1 envelope mediates the degree of vibrato from mod2, or not.
  if (mod2_vibrato_depth) {
     pitch_delta2 = ((ad2_value - 32767) * mod2_vibrato_depth) >> 11;
     if (mod1_mod2_vibrato_depth) {
        pitch_delta2 = (pitch_delta2 * ad_value) >> 16;
     }
  }

  if (quantize_vibrato) {     
	  if (modulator1_mode == 2) {
		 pitch -= pitch_delta1; 
	  } else {  
		 pitch += pitch_delta1; 
	  }    

	  if (modulator2_mode == 2) {
		 pitch -= pitch_delta2;
	  } else {
		 pitch += pitch_delta2;
	  }        
  }
  
  if (settings.pitch_quantization() == PITCH_QUANTIZATION_QUARTER_TONE) {
     pitch = (pitch + 32) & 0xffffffc0;
  } else if (settings.pitch_quantization() == PITCH_QUANTIZATION_SEMITONE) {
     pitch = (pitch + 64) & 0xffffff80;
  } else if (settings.pitch_quantization() > PITCH_QUANTIZATION_SEMITONE) {
     pitch = (pitch + 64) & 0xffffff80;
     uint8_t pitch_semitones = pitch >> 7;
     uint8_t pitch_whole_octaves = pitch_semitones / 12 ;
     uint8_t pitch_remainder_semitones = pitch_semitones - (pitch_whole_octaves * 12);
     pitch = ((pitch_whole_octaves * 12) + quant_scales[((settings.pitch_quantization() - 3) * 12) + pitch_remainder_semitones]) << 7;
  }

  // add FM
  if (meta_mod == 0) {
    pitch += settings.adc_to_fm(adc.channel(3));
  }
  
  pitch += internal_adc.value() >> 8;

  // or harmonic intervals 
  if (meta_mod == 9) {
     // Apply hysteresis to ADC reading to prevent a single bit error to move
     // the quantized pitch up and down the quantization boundary.
     uint16_t fm_adc_code = adc.channel(3);
     if ((fm_adc_code > previous_fm_adc_code + 4) ||
         (fm_adc_code < previous_fm_adc_code - 4)) {
        previous_fm_adc_code = fm_adc_code;
     } else {
        fm_adc_code = previous_fm_adc_code;
     }
     int32_t harmonic_multiplier = settings.adc_to_fm(fm_adc_code) >> 8;
//      if (harmonic_multiplier < -31) {
// 	    harmonic_multiplier = -31;
//      } else if (harmonic_multiplier > 31) {
//         harmonic_multiplier = 31;
//      }
     harmonic_multiplier = ParamClip(harmonic_multiplier, static_cast<int32_t>(-31), static_cast<int32_t>(31));
     if (harmonic_multiplier > 0) {
        pitch += (1536 * log2_table[harmonic_multiplier - 1]) >> 11;
     } else if (harmonic_multiplier < 0) {
        pitch -= (1536 * log2_table[-1 - harmonic_multiplier]) >> 11;
     }
  }
  
  // Check if the pitch has changed to cause an auto-retrigger
  int32_t pitch_delta = pitch - previous_pitch;
  if (settings.data().auto_trig &&
      (pitch_delta >= 0x40 || -pitch_delta >= 0x40)) {
    trigger_detected_flag = true;
  }
  previous_pitch = pitch;

  // Or add vibrato here
  if (!quantize_vibrato) {
	  if (modulator1_mode == 2) {
		 pitch -= pitch_delta1; 
	  } else {  
		 pitch += pitch_delta1; 
	  }    

	  if (modulator2_mode == 2) {
		 pitch -= pitch_delta2;
	  } else {
		 pitch += pitch_delta2;
	  }        
  }

  // jitter depth now settable and voltage controllable.
  // TO-DO jitter still causes pitch to sharpen slightly - why?
  int32_t vco_drift = settings.vco_drift();
  if (meta_mod == 13 || meta_mod == 17) {
     vco_drift += settings.adc_to_fm(adc.channel(3)) >> 6;
  } 
  if (vco_drift) {
//      if (vco_drift < 0) {
// 	    vco_drift = 0 ;
//      } else if (vco_drift > 127) {
//         vco_drift = 127;
//      }
     vco_drift = ParamClip(vco_drift, static_cast<int32_t>(0), static_cast<int32_t>(127));
    // now apply the jitter
    pitch +=  (jitter_source.Render(adc.channel(1) << 3) >> 8) * vco_drift;
  }

  if (metaseq_length) {
     pitch += metaseq_pitch_delta;
  }

  if (turing_length) {
     pitch += turing_pitch_delta;
  }

  // add software fine tune
  pitch += settings.fine_tune();
  
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
       ++mod1_sync_index;
       if (mod1_sync_index >= settings.GetValue(SETTING_MOD1_SYNC)) {
          envelope.Trigger(ENV_SEGMENT_ATTACK);
          mod1_sync_index = 0 ;
       }
    }
    if (settings.GetValue(SETTING_MOD2_SYNC)) {
       ++mod2_sync_index;
       if (mod2_sync_index >= settings.GetValue(SETTING_MOD2_SYNC)) {
          envelope2.Trigger(ENV_SEGMENT_ATTACK);
          mod2_sync_index = 0 ;
       }
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
  if (meta_mod == 8) {
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
  // scale the gain by the meta-sequencer parameter if applicable
  if (metaseq_length && (settings.GetValue(SETTING_METASEQ_PARAMETER_DEST) & 4)) {
     gain = (gain * metaseq_parameter) >> 7;
  }
  // clip the gain  
//   if (gain > 65535) {
//       gain = 65535;
//   }
//   else if (gain < 0) {
//       gain = 0;
//   }
  gain = ParamClip(gain, static_cast<int32_t>(0), static_cast<int32_t>(65535));

  // Voltage control of bit crushing
  uint8_t bits_value = settings.resolution();
  if (meta_mod == 14 || meta_mod == 16 || meta_mod == 17) {
     bits_value -= settings.adc_to_fm(adc.channel(3)) >> 9;
//      if (bits_value < 0) {
// 	    bits_value = 0 ;
//      } else if (bits_value > 6) {
//         bits_value = 6;
//      }
     bits_value = ParamClip(bits_value, static_cast<uint8_t>(0), static_cast<uint8_t>(6));
  }

  // Voltage control of sample rate decimation
  uint8_t sample_rate_value = settings.data().sample_rate;
  if (meta_mod == 15 || meta_mod == 16 || meta_mod == 17) {
     sample_rate_value -= settings.adc_to_fm(adc.channel(3)) >> 9;
//      if (sample_rate_value < 0) {
// 	    sample_rate_value = 0 ;
//      } else if (sample_rate_value > 6) {
//         sample_rate_value = 6;
//      }
     sample_rate_value = ParamClip(sample_rate_value, static_cast<uint8_t>(0), static_cast<uint8_t>(6));
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
