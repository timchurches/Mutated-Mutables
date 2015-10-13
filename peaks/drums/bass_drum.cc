// Copyright 2013 Olivier Gillet.
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
//
// -----------------------------------------------------------------------------
//
// 808-style bass drum.

#include "peaks/drums/bass_drum.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void BassDrum::Init() {
  pulse_up_.Init();
  pulse_down_.Init();
  attack_fm_.Init();
  resonator_.Init();
  
  pulse_up_.set_delay(0);
  pulse_up_.set_decay(3340);

  pulse_down_.set_delay(1.0e-3 * 48000);
  pulse_down_.set_decay(3072);

  attack_fm_.set_delay(4.0e-3 * 48000);
  attack_fm_.set_decay(4093);
  
  resonator_.set_punch(32768);
  resonator_.set_mode(SVF_MODE_BP);
  
  set_frequency(0);
  set_decay(32768);
  set_tone(32768);
  set_punch(65535);
  
  lp_state_ = 0;
}

int16_t BassDrum::ProcessSingleSample(uint8_t control) {
  if (control & CONTROL_GATE_RISING) {
    pulse_up_.Trigger(12 * 32768 * 0.7);
    pulse_down_.Trigger(-19662 * 0.7);
    attack_fm_.Trigger(18000);
  }
  int32_t excitation = 0;
  excitation += pulse_up_.Process();
  excitation += !pulse_down_.done() ? 16384 : 0;
  excitation += pulse_down_.Process();
  attack_fm_.Process();
  resonator_.set_frequency(frequency_ + (attack_fm_.done() ? 0 : 17 << 7));

  int32_t resonator_output = (excitation >> 4) + resonator_.Process(excitation);
  lp_state_ += (resonator_output - lp_state_) * lp_coefficient_ >> 15;
  int32_t output = lp_state_;
  CLIP(output);
  return output;
}

// randomised version
void RandomisedBassDrum::Init() {
  pulse_up_.Init();
  pulse_down_.Init();
  attack_fm_.Init();
  resonator_.Init();
  
  pulse_up_.set_delay(0);
  pulse_up_.set_decay(3340);

  pulse_down_.set_delay(1.0e-3 * 48000);
  pulse_down_.set_decay(3072);

  attack_fm_.set_delay(4.0e-3 * 48000);
  attack_fm_.set_decay(4093);
  
  resonator_.set_punch(32768);
  resonator_.set_mode(SVF_MODE_BP);
  
  set_frequency(0);
  last_frequency_ = 0;
  set_decay(32768);
  set_tone(32768);
  set_punch(65535);
  
  lp_state_ = 0;
}

int16_t RandomisedBassDrum::ProcessSingleSample(uint8_t control) {
  if (control & CONTROL_GATE_RISING) {
    // randomise parameters
    // frequency
    bool freq_up = (stmlib::Random::GetWord() > 2147483647) ? true : false ;
    int32_t randomised_frequency = freq_up ? 
                                   (last_frequency_ + (frequency_randomness_ >> 3)) :
                                   (last_frequency_ - (frequency_randomness_ >> 3));
    // constrain randomised frequency
    if (randomised_frequency < -32767) { 
      randomised_frequency = -32767; 
    } else if (randomised_frequency > 32767) { 
      randomised_frequency = 32767; 
    }
    // set new random frequency
    set_frequency(randomised_frequency) ; 
    last_frequency_ = randomised_frequency ;

    // now random excitation level and decay
    int32_t hit_random_offset = (stmlib::Random::GetSample() * hit_randomness_) >> 16;
    int32_t randomised_decay = base_decay_ + (hit_random_offset >> 2);
    // constrain randomised decay
    if (randomised_decay < 0) { 
      randomised_decay = 0; 
    } else if (randomised_decay > 65335) { 
      randomised_decay = 65335; 
    }
    set_decay(randomised_decay);
    pulse_up_.Trigger(12 * 32768 * 0.7);
    pulse_down_.Trigger(-19662 * 0.7);
    attack_fm_.Trigger(18000);
  }
  int32_t excitation = 0;
  excitation += pulse_up_.Process();
  excitation += !pulse_down_.done() ? 16384 : 0;
  excitation += pulse_down_.Process();
  attack_fm_.Process();
  resonator_.set_frequency(frequency_ + (attack_fm_.done() ? 0 : 17 << 7));

  int32_t resonator_output = (excitation >> 4) + resonator_.Process(excitation);
  lp_state_ += (resonator_output - lp_state_) * lp_coefficient_ >> 15;
  int32_t output = lp_state_;
  CLIP(output);
  return output;
}


}  // namespace peaks
