// Copyright 2013 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
 
#ifndef PEAKS_DRUMS_CYMBAL_H_
#define PEAKS_DRUMS_CYMBAL_H_

#include "stmlib/stmlib.h"

#include "peaks/drums/svf.h"
#include "peaks/drums/excitation.h"

#include "peaks/gate_processor.h"

namespace peaks {

class Cymbal {
 public:
  Cymbal() { }
  ~Cymbal() { }

  void Init();
  void Process(const GateFlags* gate_flags, int16_t* out, size_t size);
  void Configure(uint16_t* parameter, ControlMode control_mode) {
	  if (control_mode == CONTROL_MODE_HALF) {
     	set_pitch(32767);
		set_clip(32767); 
   		set_xfade(parameter[0]); 
		set_decay(parameter[1]);
	  }else{
		set_pitch(parameter[0]);
		set_clip(parameter[1]); 
		set_xfade(parameter[2]);
		set_decay(parameter[3]);
	  }
  }
  
  
  void set_pitch(uint16_t pitch) {
	pitch_ = pitch<<7;
  }

  void set_clip(uint16_t clip) {
  clip_=clip;
  }

  void set_xfade(int32_t xfade) {
  xfade_=xfade>>1;
  }

  void set_decay(uint16_t decay) {
  vca_envelope_.set_decay(4092+  (decay >> 14));
   }
  
 
 private:
  Svf svf_hat_noise1_;
  Svf svf_hat_noise2_;
  Svf vca_coloration_;
  Excitation vca_envelope_;

  
  uint32_t pitch_;
  uint32_t phase_;
  uint16_t clip_;
  int32_t xfade_;
  uint32_t rng_state_;
  uint32_t cym_phase_[6];


  DISALLOW_COPY_AND_ASSIGN(Cymbal);
};

}  // namespace peaks

#endif  // PEAKS_DRUMS_CYMBAL_H_
