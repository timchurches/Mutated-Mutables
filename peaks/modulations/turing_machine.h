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
// Mini sequencer.

#ifndef PEAKS_MODULATIONS_TURING_MACHINE_H_
#define PEAKS_MODULATIONS_TURING_MACHINE_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "peaks/gate_processor.h"

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

namespace peaks {

class TuringMachine {
 public:
  TuringMachine() { }
  ~TuringMachine() { }
  
  void Init() {
    turing_length_ = 0;
    turing_prob_ = 0;
    turing_offset_ = 0;
    turing_span_ = 0;
    turing_shift_register_ = stmlib::Random::GetWord();
    turing_pitch_delta_ = 0;
    turing_lsb_ = turing_shift_register_ & static_cast<uint32_t>(1);
    turing_remainder_lsb_ = false;    
  }
    
  inline void set_turing_length(int16_t value) {
    turing_length_ = value >> 11;
    if (turing_length_ < 2) {
      turing_length_ = 2 ;
    }
    if (turing_length_ > 32) {
      turing_length_ = 32 ;
    }
  }
 
  inline void set_turing_prob(int16_t value) {
    turing_prob_ = value;
  }

  inline void set_turing_offset(int16_t value) {
    turing_offset_ = value;
  }
  
  inline void set_turing_span(int16_t value) {
    turing_span_ = value;
  }
 
  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      set_turing_prob(parameter[0] - 32768);
      set_turing_length(parameter[1] - 32768);
    } else {
      set_turing_prob(parameter[0] - 32768);
      set_turing_length(parameter[1] - 32768);
      set_turing_offset(parameter[2] - 32768);
      set_turing_span(parameter[3] - 32768);
    }
  }
  
  inline int16_t ProcessSingleSample(uint8_t control) {
    if (control & CONTROL_GATE_RISING) {
        // read the LSB
        turing_lsb_ = turing_shift_register_ & static_cast<uint32_t>(1);
        // read the LSB in the remainder of the shift register
        if (turing_length_ < 32) {
           turing_remainder_lsb_ = turing_shift_register_ & (static_cast<uint32_t>(1) << turing_length_);
        }
        // rotate the shift register
        turing_shift_register_ = turing_shift_register_ >> 1;
        // add back the LSB into the MSB postion
        if (turing_lsb_) {
           turing_shift_register_ |= (static_cast<uint32_t>(1) << (turing_length_ - 1));
        } else {
           turing_shift_register_ &= (~(static_cast<uint32_t>(1) << (turing_length_ - 1)));
        }
        // add back the LSB to the remainder of the shift register
        if (turing_length_ < 32) {
           if (turing_remainder_lsb_) {
              turing_shift_register_ |= (static_cast<uint32_t>(1) << 31);
           } else {
              turing_shift_register_ &= (~(static_cast<uint32_t>(1) << 31));
           }
        }
        // decide whether to flip the LSB
        turing_prob_ = turing_prob_ >> 8;
        // Clip at zero and 127
          // turing_prob = ParamClip(turing_prob, static_cast<int16_t>(0), static_cast<int16_t>(127));

        if (static_cast<uint16_t>(stmlib::Random::GetWord() >> 17) < turing_prob_) {
           // bit-flip the LSB
           turing_shift_register_ = turing_shift_register_ ^ static_cast<uint32_t>(1) ;
        }

// Up to here

       // read the window and calculate pitch increment
        // int16_t turing_window = settings.GetValue(SETTING_TURING_WINDOW);
        // if (meta_mod == 12) {
           // add the FM CV amount, offset by 2
	    //   turing_window += (settings.adc_to_fm(adc.channel(3)) >> 7) + 2;
        // }
        // Clip at zero and 36
        // turing_window = ParamClip(turing_window, static_cast<int16_t>(0), static_cast<int16_t>(36));

        turing_byte_ = turing_shift_register_ & static_cast<uint32_t>(0xFF);
        // uint8_t turing_value = (turing_byte * static_cast<uint8_t>(turing_window)) >> 8;
    }
    return turing_byte_ << 8;
  }
  
 private:
  int16_t turing_length_;
  int16_t turing_prob_;
  int16_t turing_offset_;
  int16_t turing_span_;
  uint32_t turing_shift_register_;
  int32_t turing_pitch_delta_;
  bool turing_lsb_;
  bool turing_remainder_lsb_;
  uint32_t turing_byte_;
  
  DISALLOW_COPY_AND_ASSIGN(TuringMachine);
};

}  // namespace peaks

#endif  // PEAKS_MODULATIONS_TURING_MACHINE_H_
