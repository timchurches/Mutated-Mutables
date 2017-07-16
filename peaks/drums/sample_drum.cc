// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com), Tom Burns (tom@burns.ca)
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
// Number station.

#include "peaks/drums/sample_drum.h"

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

const uint8_t *sample_wav[] = {
  &wav_shaker[0],
  &wav_clap[0],
  &wav_snap[0],
  &wav_wood[0],
  &wav_bells[0],
  &wav_tambourine[0],
  &wav_scissors[0]
};

const uint16_t sample_length[] = {
  WAV_SHAKER_SIZE,
  WAV_CLAP_SIZE,
  WAV_SNAP_SIZE,
  WAV_WOOD_SIZE,
  WAV_BELLS_SIZE,
  WAV_TAMBOURINE_SIZE,
  WAV_SCISSORS_SIZE
};

using namespace stmlib;

void SampleDrum::Init() {
  deglitch_.Init();
  deglitch_.set_delay(0);
  deglitch_.set_decay(20);

  phase_ = 0;
  sample_ = 0;
  playing_sample_ = 0;
  gate_ = false;
  distortion_phase_ = 0;
}

void SampleDrum::Process(
    const GateFlags* gate_flags, int16_t* out, size_t size) {

//  int32_t amplitude;

  while (size--) {
    GateFlags gate_flag = *gate_flags++;
    if (gate_flag & GATE_FLAG_RISING) {
      gate_ = true;
      phase_ = 0;
      if (sample_ != 0xffff) {
        playing_sample_ = sample_;
      } else {
        playing_sample_ = ((uint16_t) stmlib::Random::GetSample()) % 7;
      }
    }

    int16_t output = 0;
    uint16_t integral = phase_ >> 16;
    uint16_t fractional = phase_ & 0xffff;

    if (gate_ && integral < sample_length[playing_sample_]-1) {
      int32_t a = static_cast<int32_t>(sample_wav[playing_sample_][integral]);
      int32_t b = static_cast<int32_t>(sample_wav[playing_sample_][integral + 1]);

      output = (a << 8) + (((b - a) * fractional) >> 8);
      output -= 32768;
      phase_ += pitch_shift_;
    } else {
      gate_ = false;
    }

    if (distortion_phase_ == 0) {
      distortion_phase_ = distortion_;
      distortion_sample_ = output;
    } else {
      distortion_phase_--;
    }

    *out++ = distortion_sample_;
  }
}

}  // namespace peaks
