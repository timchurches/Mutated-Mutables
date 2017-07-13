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

#include "peaks/drums/high_hat.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void HighHat::Init() {
  noise_.Init();
  noise_.set_frequency(105 << 7);  // 8kHz
  noise_.set_resonance(24000);

  // vca_coloration_.Init();
  // vca_coloration_.set_frequency(110 << 7);  // 13kHz
  // vca_coloration_.set_resonance(0);

  vca_envelope_.Init();
  vca_envelope_.set_delay(0);
  vca_envelope_.set_decay(4093);
}

void HighHat::Process(const GateFlags* gate_flags, int16_t* out, size_t size) {
  while (size--) {
    GateFlags gate_flag = *gate_flags++;

    if (gate_flag & GATE_FLAG_RISING &&
      (open_ || (!open_ && !(gate_flag & GATE_FLAG_AUXILIARY_RISING)))) {

      // randomise parameters
      // frequency
      uint32_t random_value = stmlib::Random::GetWord();
      bool freq_up = (random_value > 2147483647) ? true : false ;
      int32_t randomised_frequency = freq_up ?
                                    (last_frequency_ + (frequency_randomness_ >> 2)) :
                                    (last_frequency_ - (frequency_randomness_ >> 2));

      // Check if we haven't walked out-of-bounds, and if so, reverse direction on last step
      if (randomised_frequency < 0 || randomised_frequency > 65535) {
        // flip the direction
        freq_up = !freq_up ;
        randomised_frequency = freq_up ?
                                     (last_frequency_ + (frequency_randomness_ >> 2)) :
                                     (last_frequency_ - (frequency_randomness_ >> 2));
      }

      // constrain randomised frequency - probably not necessary
      if (randomised_frequency < 0) {
        randomised_frequency = 0;
      } else if (randomised_frequency > 65535) {
        randomised_frequency = 65535;
      }

      // set new random frequency
      set_frequency(randomised_frequency);
      last_frequency_ = randomised_frequency;

      // decay
      random_value = stmlib::Random::GetWord() ;
      freq_up = (random_value > 2147483647) ? true : false ;
      randomised_frequency = freq_up ?
                                     (last_decay_ + (decay_randomness_ >> 2)) :
                                     (last_decay_ - (decay_randomness_ >> 2));

      // Check if we haven't walked out-of-bounds, and if so, reverse direction on last step
      if (randomised_frequency < 0 || randomised_frequency > 65535) {
        // flip the direction
        freq_up = !freq_up ;
        randomised_frequency = freq_up ?
                                   (last_decay_ + (decay_randomness_ >> 2)) :
                                   (last_decay_ - (decay_randomness_ >> 2));
      }

      // constrain randomised frequency - probably not necessary
      if (randomised_frequency < 0) {
        randomised_frequency = 0;
      } else if (randomised_frequency > 65535) {
        randomised_frequency = 65535;
      }

      // set new random decay
      set_decay(randomised_frequency) ;
      last_decay_ = randomised_frequency ;

      // Hit it!
      vca_envelope_.Trigger(32768 * 15);
    }

    phase_[0] += 48318382;
    phase_[1] += 71582788;
    phase_[2] += 37044092;
    phase_[3] += 54313440;
    phase_[4] += 66214079;
    phase_[5] += 93952409;

    int16_t noise = 0;
    noise += phase_[0] >> 31;
    noise += phase_[1] >> 31;
    noise += phase_[2] >> 31;
    noise += phase_[3] >> 31;
    noise += phase_[4] >> 31;
    noise += phase_[5] >> 31;
    noise <<= 12;

    // Run the SVF at the double of the original sample rate for stability.
    int32_t filtered_noise = 0;
    filtered_noise += noise_.Process<SVF_MODE_BP>(noise);
    // filtered_noise += noise_.Process(noise);

    // The 808-style VCA amplifies only the positive section of the signal.
    if (filtered_noise < 0) {
      filtered_noise = 0;
    } else if (filtered_noise > 32767) {
      filtered_noise = 32767;
    }

    int32_t envelope = vca_envelope_.Process() >> 4;
    int32_t vca_noise = envelope * filtered_noise >> 14;
    CLIP(vca_noise);
    *out++ = vca_noise;
  }
  // int32_t hh = 0;
  // hh += vca_coloration_.Process<SVF_MODE_HP>(vca_noise);
  // hh += vca_coloration_.Process<SVF_MODE_HP>(vca_noise);
  // hh <<= 1;
  // hh <<= 2;
  // CLIP(hh);
  // return hh;
}


}  // namespace peaks
