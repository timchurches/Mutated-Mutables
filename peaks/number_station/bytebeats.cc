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
  // temporary vars
  uint32_t p ;
  uint32_t q ;
  uint8_t j ;
  
  uint16_t bytepitch = (65535 - frequency_) >> 11 ; // was 12
  if (bytepitch < 1) {
    bytepitch = 1;
  }
  equation_index_ = p2_ >> 13 ;
  uint8_t size = kBlockSize / kDownsample;
  while (size--) {
    for (uint8_t i = 0; i < kDownsample; ++i) {
      uint8_t control = input_buffer->ImmediateRead();
      if (control & CONTROL_GATE_RISING) {
        // (void)0; // noop
        phase_ = 0;
        t_ = 0 ;
        // ++equation_index_;
        // if (equation_index_ > kMaxEquationIndex) {
        //   equation_index_ = 0 ;
        // }
      }
      if (control & CONTROL_GATE) {
        (void)0; // noop
      }
    }

    ++phase_;
    if (phase_ % bytepitch == 0) ++t_; 
    switch (equation_index_) {
      case 0:
        // from http://royal-paw.com/2012/01/bytebeats-in-c-and-python-generative-symphonies-from-extremely-small-programs/
        // (atmospheric, hopeful)
        p0 = p0_ >> 9; // was 9
        p1 = p1_ >> 9; // was 11
        sample = ( ( ((t_*3) & (t_>>10)) | ((t_*p0) & (t_>>10)) | ((t_*10) & ((t_>>8)*p1) & 128) ) & 0xFF) << 8;
        break;
      case 1:
        p0 = p0_ >> 11;
        p1 = p1_ >> 11;
        // p2 = p2_ >> 11;
        // equation by stephth via https://www.youtube.com/watch?v=tCRPUv8V22o at 3:38
        sample = ((((t_*p0) & (t_>>4)) | ((t_*5) & (t_>>7)) | ((t_*p1) & (t_>>10))) & 0xFF) << 8;
        // sample = ((((t_*p0) & (t_>>4)) | ((t_*p2) & (t_>>7)) | ((t_*p1) & (t_>>10))) & 0xFF) << 8;
        break;
      case 2: 
        p0 = p0_ >> 12;
        p1 = p1_ >> 12;
        // This one is from http://www.reddit.com/r/bytebeat/comments/20km9l/cool_equations/ (t>>13&t)*(t>>8)
        sample = ( (((t_ >> p0) & t_) * (t_ >> p1)) & 0xFF) << 8 ;
        break;
      case 3: 
        p0 = p0_ >> 11;
        p1 = p1_ >> 9;
        // This one is the second one listed at from http://xifeng.weebly.com/bytebeats.html
        sample = ((( (((((t_ >> p0) | t_) | (t_ >> p0)) * 10) & ((5 * t_) | (t_ >> 10)) ) | (t_ ^ (t_ % p1)) ) & 0xFF)) << 8 ;
        break;
      case 4: 
        p0 = p0_ >> 12; // was 9
        p1 = p1_ >> 12; // was 11
        //  BitWiz Transplant from Equation Composer Ptah bank        
        // run at twice normal sample rate
        for (j = 0; j < 2; ++j) {
          sample = t_*(((t_>>p1)^((t_>>p1)-1)^1)%p0) ; 
          if (j == 0) ++t_ ; 
        }
        break;
      case 5:
        p0 = p0_ >> 11;
        p1 = p1_ >> 9;
        // Arpeggiation from Equation Composer Khepri bank
        // run at twice normal sample rate
        // for (uint8_t j = 0; j < 2; ++j) {
          p = ((t_/(1236+p0)) % 128) & ((t_>>(p1>>5))*p1);
          q = (t_/(t_/((500*p1) % 5) + 1)) % p;
          sample = (t_>>q>>(p1>>5)) + (t_/(t_>>((p1>>5)&12))>>p);
        //  if (j == 0) ++t_ ; 
        // }
        break;
      case 6:
        p0 = p0_ >> 9;
        p1 = p1_ >> 10; // was 9
        // The Smoker from Equation Composer Khepri bank
        sample = sample ^ (t_>>(p1>>4)) >> ((t_/6988*t_%(p0+1))+(t_<<t_/(p1 * 4)));
        break;
      default:
        p0 = p0_ >> 9;
        p1 = t_ % p1_ ;
        // // run at twice normal sample rate
        // for (j = 0; j < 2; ++j) {
          // Warping overtone echo drone, from BitWiz
          sample = ((t_&p0)-(t_%p1))^(t_>>7);  
          // sample = t_*(((t_>>p1)^((t_>>p1)-1)^1)%p0) ; 
          // if (j == 0) ++t_ ; 
        // }
        break;      
    }
    CLIP(sample)
    output_buffer->Overwrite(sample);
  }
}

}  // namespace peaks
