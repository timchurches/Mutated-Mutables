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

#ifndef PEAKS_DRUMS_SAMPLE_DRUM_H_
#define PEAKS_DRUMS_SAMPLE_DRUM_H_

#include "stmlib/stmlib.h"
#include "peaks/gate_processor.h"
#include "peaks/drums/excitation.h"

namespace peaks {

class SampleDrum {
 public:
  SampleDrum() { }
  ~SampleDrum() { }

  void Init();
  void Process(const GateFlags* gate_flags, int16_t* out, size_t size);

  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      set_sample(parameter[0]);
      set_tone(parameter[1]);
      set_distortion(0);
    } else {
      set_sample(parameter[0]);
      set_tone(parameter[1]);
      set_distortion(parameter[2]);
    }
  }

  inline void set_sample(uint16_t sample) {
    sample_ = sample >> 13;
    if (sample_ == 7) {
      // pick a random sample each trigger
      sample_ = 0xffff;
    }
  }

  inline void set_tone(uint16_t tone) {
    // every 65536 == 1 sample forward.
    // to scan from 2 octaves down to 2 octaves above we should scale from
    // 16384 to 65536 to 262144
    //

    // tone 0, speed 4096
    // tone 32768, speed 65536

    // 4096 = x*0 + y
    // 65536 = x*(32768) + 4096
    //

    // 65536 = 3.75*32768

    pitch_shift_ = tone;// << 1;
  }

  inline void set_distortion(uint16_t distortion) {
    distortion_ = distortion >> 8;
  }

  inline bool gate() const { return gate_; }

 private:
  uint16_t sample_;
  uint16_t playing_sample_;

  uint32_t pitch_shift_;
  int32_t distortion_;
  uint32_t phase_;
  uint32_t distortion_phase_;
  int16_t distortion_sample_;

  Excitation deglitch_;

  bool gate_;

  DISALLOW_COPY_AND_ASSIGN(SampleDrum);
};

}  // namespace peaks

#endif  // PEAKS_NUMBER_STATION_NUMBER_STATION_H_
