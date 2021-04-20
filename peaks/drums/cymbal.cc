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

#include "peaks/drums/cymbal.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;



void Cymbal::Init() {

  vca_coloration_.Init();
  vca_coloration_.set_frequency(110 << 7);  // 13kHz
  vca_coloration_.set_resonance(0);
  
  vca_envelope_.Init();
  vca_envelope_.set_delay(0);
  vca_envelope_.set_decay(4093);

	// SVF for multifreq signal
  svf_hat_noise1_.Init();
  svf_hat_noise1_.set_frequency(105 << 7); // 8kHz
  svf_hat_noise1_.set_resonance(24000);

	// SVF for PNR signal
  svf_hat_noise2_.Init();
  svf_hat_noise2_.set_frequency(105 << 7); // 8kHz
  svf_hat_noise2_.set_resonance(24000);
 
}



void Cymbal::Process(const GateFlags* gate_flags, int16_t* out, size_t size) {
  while (size--) {
    GateFlags gate_flag = *gate_flags++;
    if (gate_flag & GATE_FLAG_RISING) {
		vca_envelope_.Trigger(32768 * 15);
    }

	// Pseudo Random Noise Generation
    
    phase_ += 93886874*20;
    if (phase_ < 93886874*20) {
      rng_state_ = rng_state_ * 1664525L + 1013904223L;
    }
 
  
    cym_phase_[0] += 48252847 - 4194176 + pitch_ ;
    cym_phase_[1] += 71517253 - 4194176 + pitch_ ;
    cym_phase_[2] += 36978557 - 4194176 + pitch_ ;
    cym_phase_[3] += 54247905 - 4194176 + pitch_ ;
    cym_phase_[4] += 66148544 - 4194176 + pitch_ ;
    cym_phase_[5] += 93886874 - 4194176 + pitch_ ;

    int16_t noise = 0;
    noise += cym_phase_[0] >> 31;
    noise += cym_phase_[1] >> 31;
    noise += cym_phase_[2] >> 31;
    noise += cym_phase_[3] >> 31;
    noise += cym_phase_[4] >> 31;
    noise += cym_phase_[5] >> 31;
    noise <<= 12;
  
    int32_t filtered_noise1 = svf_hat_noise1_.Process<SVF_MODE_BP>(noise);;
	filtered_noise1 <<= 2;

    // The 808-style VCA clipping 
    if (filtered_noise1 < clip_ - 32767) {
      filtered_noise1 =  clip_ - 32767;
    } else if (filtered_noise1 > 32767) {
      filtered_noise1 = 32767;
    }
  
	// Random Noise Processing
    int32_t rng_noise = (rng_state_ >> 16) - 32768;
	int32_t filtered_noise2 = svf_hat_noise2_.Process<SVF_MODE_BP>(rng_noise>>1);
//	CLIP(filtered_noise2);

	
    int32_t envelope = vca_envelope_.Process() >> 4;
	int32_t vca_noise1 = (envelope * filtered_noise1 >> 15);
	int32_t vca_noise2 = (envelope * filtered_noise2 >> 15);
	CLIP(vca_noise1);
	CLIP(vca_noise2);

	// crossfading pseudo random noise and multifrequency signal
	int32_t vca_noise = vca_noise1 + ((vca_noise2 - vca_noise1) * xfade_ >> 15);
	int32_t hh = vca_coloration_.Process<SVF_MODE_HP>(vca_noise);
    CLIP(hh);

    *out++ = hh;
  }
}



}  // namespace peaks
