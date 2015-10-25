// Copyright 2013 Olivier Gillet, 2015 Tim Churches
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
// Modifications: Tim Churches (tim.churches@gmail.com)
// Modifications may be determined by examining the differences between the last commit 
// by Olivier Gillet (pichenettes) and the HEAD commit at 
// https://github.com/timchurches/Mutated-Mutables/tree/master/peaks 
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
// 808-style HH.
 
#ifndef PEAKS_DRUMS_HIGH_HAT_H_
#define PEAKS_DRUMS_HIGH_HAT_H_

#include "stmlib/stmlib.h"

#include "peaks/drums/svf.h"
#include "peaks/drums/excitation.h"

#include "peaks/gate_processor.h"

namespace peaks {

class HighHat {
 public:
  HighHat() { }
  ~HighHat() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      set_frequency(parameter[0]);
      base_frequency_ = parameter[0];
      last_frequency_ = base_frequency_;
      set_decay(parameter[1]);
      base_decay_ = parameter[1];
      last_decay_ = base_decay_;      
    } else {
      set_frequency(parameter[0]);
      base_frequency_ = parameter[0];
      last_frequency_ = base_frequency_;
      set_decay(parameter[1]);
      base_decay_ = parameter[1];
      last_decay_ = base_decay_;      
      set_frequency_randomness(parameter[2]);
      set_decay_randomness(parameter[3]);
    }
  }

  void set_frequency(uint16_t frequency) {
    noise_.set_frequency((105 << 7) - ((32767 - frequency)  >> 6));  // 8kHz
  }

  void set_decay(uint16_t decay) {
    uint16_t decay_value = 4065 + (decay >> 11) ;
    if (decay_value > 4095) {
      decay_value = 4095 ;
    }
    vca_envelope_.set_decay(decay_value);
  }

  void set_frequency_randomness(uint16_t frequency_randomness) {
    frequency_randomness_ = frequency_randomness;
  }

  void set_decay_randomness(uint16_t decay_randomness) {
    decay_randomness_ = decay_randomness;
  }

  inline void set_open(bool open) {
    open_ = open;
  }
  
 private:
  Svf noise_;
  // Svf vca_coloration_;
  Excitation vca_envelope_;
  
  uint32_t phase_[6];

  uint16_t frequency_randomness_ ;
  uint16_t decay_randomness_ ;
  int32_t randomised_hit_  ;

  uint16_t base_frequency_ ;
  uint16_t last_frequency_ ;
  uint16_t base_decay_ ;
  uint16_t last_decay_ ;
  
  bool open_ ;

  DISALLOW_COPY_AND_ASSIGN(HighHat);
};

}  // namespace peaks

#endif  // PEAKS_DRUMS_HIGH_HAT_H_
