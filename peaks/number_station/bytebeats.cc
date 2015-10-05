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
// Byte beats

#include "peaks/number_station/bytebeats.h"

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

const uint8_t kDownsample = 4;
const uint8_t kUpsample = 2;
const uint8_t kMaxEquationIndex = 1;

using namespace stmlib;

void ByteBeats::Init() {
  frequency_ = 32678;
  phase_ = 0;
  p0_ = 32678;
  p1_ = 32678;
  p2_ = 0;
  equation_index_ = 0;
}

void ByteBeats::FillBuffer(
    InputBuffer* input_buffer,
    OutputBuffer* output_buffer) {
  uint32_t p0 = 0;
  uint32_t p1 = 0;
  // uint32_t p2 = 0;
  int32_t sample = 0;
  uint16_t bytepitch = (65535 - frequency_) >> 11 ; // was 12
  if (bytepitch < 1) {
    bytepitch = 1;
  }
  equation_index_ = p2_ >> 14 ;
  uint8_t size = kBlockSize / kDownsample;
  while (size--) {
    for (uint8_t i = 0; i < kDownsample; ++i) {
      uint8_t control = input_buffer->ImmediateRead();
      if (control & CONTROL_GATE_RISING) {
        (void)0; // noop
        // ++equation_index_;
        // if (equation_index_ > kMaxEquationIndex) {
        //   equation_index_ = 0 ;
        // }
      }
      if (control & CONTROL_GATE) {
        (void)0; // noop
      }
    }

    // ++phase_;
    // if (phase_ % bytepitch == 0) ++t_; 
    switch (equation_index_) {
      case 0:
        p0 = p0_ >> 9;
        p1 = p1_ >> 11;
        // p2 = p2_ >> 11;
        for (uint8_t i = 0; i < kUpsample; ++i) {
          ++phase_;
          if (phase_ % bytepitch == 0) ++t_; 
          // from http://royal-paw.com/2012/01/bytebeats-in-c-and-python-generative-symphonies-from-extremely-small-programs/
          // (atmospheric, hopeful)
          sample = ( ( ((t_*3) & (t_>>10)) | ((t_*p0) & (t_>>10)) | ((t_*10) & ((t_>>8)*p1) & 128) ) & 0xFF) << 8;
          CLIP(sample)
          output_buffer->Overwrite(sample);
        }
        break;
      case 1:
        p0 = p0_ >> 11;
        p1 = p1_ >> 11;
        // p2 = p2_ >> 11;
        for (uint8_t i = 0; i < kUpsample; ++i) {
          ++phase_;
          if (phase_ % bytepitch == 0) ++t_; 
          // equation by stephth via https://www.youtube.com/watch?v=tCRPUv8V22o at 3:38
          sample = ((((t_*p0) & (t_>>4)) | ((t_*5) & (t_>>7)) | ((t_*p1) & (t_>>10))) & 0xFF) << 8;
          CLIP(sample)
          output_buffer->Overwrite(sample);
        }
        break;
      case 2: 
        p0 = p0_ >> 12;
        p1 = p1_ >> 12;
        for (uint8_t i = 0; i < kUpsample; ++i) {
          ++phase_;
          if (phase_ % bytepitch == 0) ++t_; 
          // This one is from http://www.reddit.com/r/bytebeat/comments/20km9l/cool_equations/ (t>>13&t)*(t>>8)
          sample = ( (((t_ >> p0) & t_) * (t_ >> p1)) & 0xFF) << 8 ;
          CLIP(sample)
          output_buffer->Overwrite(sample);
        }
        break;
      case 3: 
        p0 = p0_ >> 11;
        p1 = p1_ >> 8;
        for (uint8_t i = 0; i < kUpsample; ++i) {
          ++phase_;
          if (phase_ % bytepitch == 0) ++t_; 
          // This one is the second one listed at from http://xifeng.weebly.com/bytebeats.html
          sample = ((( (((((t_ >> p0) | t_) | (t_ >> p0)) * 10) & ((5 * t_) | (t_ >> 10)) ) | (t_ ^ (t_ % p1)) ) & 0xFF)) << 8 ;
          CLIP(sample)
          output_buffer->Overwrite(sample);
        }          
        break;
    }
    // CLIP(sample)
    // output_buffer->Overwrite(sample);
  }
}

}  // namespace peaks
