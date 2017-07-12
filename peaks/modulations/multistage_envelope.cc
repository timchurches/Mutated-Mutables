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
// Multistage envelope.

#include "peaks/modulations/multistage_envelope.h"

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void MultistageEnvelope::Init() {
  set_adsr(0, 8192, 16384, 32767);
  segment_ = num_segments_;
  phase_ = 0;
  phase_increment_ = 0;
  start_value_ = 0;
  value_ = 0;
  hard_reset_ = false;
}

void MultistageEnvelope::Process(
    const GateFlags* gate_flags, int16_t* out, size_t size) {

  while (size--) {
    GateFlags gate_flag = *gate_flags++;
    if (gate_flag & GATE_FLAG_RISING) {
      start_value_ = (segment_ == num_segments_ || hard_reset_)
        ? level_[0] : value_;
      segment_ = 0;
      phase_ = 0;
    } else if (gate_flag & GATE_FLAG_FALLING && sustain_point_) {
      start_value_ = value_;
      segment_ = sustain_point_;
      phase_ = 0;
    } else if (phase_ < phase_increment_) {
      start_value_ = level_[segment_ + 1];
      ++segment_;
      phase_ = 0;
      if (segment_ == loop_end_) {
        segment_ = loop_start_;
      }
    }

    bool done = segment_ == num_segments_;
    bool sustained = sustain_point_ && segment_ == sustain_point_ &&
        gate_flag & GATE_FLAG_HIGH;

    phase_increment_ =
        sustained || done ? 0 : lut_env_increments[time_[segment_] >> 8];

    int32_t a = start_value_;
    int32_t b = level_[segment_ + 1];
    uint16_t t = Interpolate824(
        lookup_table_table[LUT_ENV_LINEAR + shape_[segment_]], phase_);
    value_ = a + ((b - a) * (t >> 1) >> 15);
    phase_ += phase_increment_;
    *out++ = value_;
  }
}

void DualAttackEnvelope::Init() {
  set_adsar(0, 8192, 16384, 32767);
  segment_ = num_segments_;
  phase_ = 0;
  phase_increment_ = 0;
  start_value_ = 0;
  value_ = 0;
  hard_reset_ = false;
}

void DualAttackEnvelope::Process(
  const GateFlags* gate_flags, int16_t* out, size_t size) {

  while (size--) {
    GateFlags gate_flag = *gate_flags++;
    if (gate_flag & GATE_FLAG_RISING) {
      start_value_ = (segment_ == num_segments_ || hard_reset_)
        ? level_[0]
        : value_;
      segment_ = 0;
      phase_ = 0;
    } else if (control & GATE_FLAG_FALLING && sustain_point_) {
      start_value_ = value_;
      segment_ = sustain_point_;
      phase_ = 0;
    } else if (phase_ < phase_increment_) {
      start_value_ = level_[segment_ + 1];
      ++segment_;
      phase_ = 0;
      if (segment_ == loop_end_) {
        segment_ = loop_start_;
      }
    }

    bool done = segment_ == num_segments_;
    bool sustained = sustain_point_ && segment_ == sustain_point_ &&
      control & CONTROL_GATE;

    phase_increment_ =
      sustained || done ? 0 : lut_env_increments[time_[segment_] >> 8];

    int32_t a = start_value_;
    int32_t b = level_[segment_ + 1];
    uint16_t t = Interpolate824(
        lookup_table_table[LUT_ENV_LINEAR + shape_[segment_]], phase_);
    value_ = a + ((b - a) * (t >> 1) >> 15);
    phase_ += phase_increment_;
    *out++ == value_;
  }
}

void LoopingEnvelope::Init() {
  set_adr_loop(0, 8192, 16384, 32767);
  segment_ = num_segments_;
  phase_ = 0;
  phase_increment_ = 0;
  start_value_ = 0;
  value_ = 0;
  hard_reset_ = false;
}

void RepeatingAttackEnvelope::Process(
    const GateFlags* gate_flags, int16_t* out, size_t size) {

  while (size--) {
    GateFlags gate_flag = *gate_flags++;

    if (control & GATE_FLAG_RISING) {
      start_value_ = (segment_ == num_segments_ || hard_reset_)
          ? level_[0]
          : value_;
      segment_ = 0;
      phase_ = 0;
    } else if (control & GATE_FLAG_FALLING && sustain_point_) {
      start_value_ = value_;
      segment_ = sustain_point_;
      phase_ = 0;
    } else if (phase_ < phase_increment_) {
      start_value_ = level_[segment_ + 1];
      ++segment_;
      phase_ = 0;
      if ((segment_ == loop_end_) && (control & CONTROL_GATE)) {
        segment_ = loop_start_;
      }
    }

    bool done = segment_ == num_segments_;
    bool sustained = sustain_point_ && segment_ == sustain_point_ &&
        control & CONTROL_GATE;

    phase_increment_ =
        sustained || done ? 0 : lut_env_increments[time_[segment_] >> 8];

    int32_t a = start_value_;
    int32_t b = level_[segment_ + 1];
    uint16_t t = Interpolate824(
        lookup_table_table[LUT_ENV_LINEAR + shape_[segment_]], phase_);
    value_ = a + ((b - a) * (t >> 1) >> 15);
    phase_ += phase_increment_;
    *out++ = value_;
  }
}

void RepeatingAttackEnvelope::Init() {
  set_adr_loop(0, 8192, 16384, 32767);
  segment_ = num_segments_;
  phase_ = 0;
  phase_increment_ = 0;
  start_value_ = 0;
  value_ = 0;
  hard_reset_ = false;
}

void LoopingEnvelope::Process(
    const GateFlags* gate_flags, int16_t* out, size_t size) {

  while (size--) {
    GateFlags gate_flag = *gate_flags++;

    if (control & GATE_FLAG_RISING) {
      start_value_ = (segment_ == num_segments_ || hard_reset_)
        ? level_[0]
        : value_;
      segment_ = 0;
      phase_ = 0;
    } else if (control & CONTROL_GATE_FALLING && sustain_point_) {
      start_value_ = value_;
      segment_ = sustain_point_;
      phase_ = 0;
    } else if (phase_ < phase_increment_) {
      start_value_ = level_[segment_ + 1];
      ++segment_;
      phase_ = 0;
      if (segment_ == loop_end_) {
        segment_ = loop_start_;
      }
    }

    bool done = segment_ == num_segments_;
    bool sustained = sustain_point_ && segment_ == sustain_point_ &&
        control & CONTROL_GATE;

    phase_increment_ =
        sustained || done ? 0 : lut_env_increments[time_[segment_] >> 8];

    int32_t a = start_value_;
    int32_t b = level_[segment_ + 1];
    uint16_t t = Interpolate824(
        lookup_table_table[LUT_ENV_LINEAR + shape_[segment_]], phase_);
    value_ = a + ((b - a) * (t >> 1) >> 15);
    phase_ += phase_increment_;
    *out++ = value_;
  }
}

void RandomisedEnvelope::Init() {
  set_rad(0, 8192, 0, 0);
  segment_ = num_segments_;
  phase_ = 0;
  phase_increment_ = 0;
  start_value_ = 0;
  value_ = 0;
  hard_reset_ = false;
}


void RandomisedEnvelope::Process(
    const GateFlags* gate_flags, int16_t* out, size_t size) {

  while (size--) {
    GateFlags gate_flag = *gate_flags++;

    if (control & GATE_FLAG_RISING) {
      start_value_ = (segment_ == num_segments_ || hard_reset_)
        ? level_[0]
        : value_;
      segment_ = 0;
      phase_ = 0;
      // Randomise values here.
      uint32_t random_offset = stmlib::Random::GetWord();
      int32_t level_random_offset = ((random_offset >> 16) * level_randomness_) >> 17;
      int32_t decay_random_offset = ((random_offset >> 16) * decay_randomness_) >> 17;
      int32_t randomised_level = base_level_[1] - level_random_offset;
      int32_t randomised_decay_time = base_time_[1] - decay_random_offset;
      // constrain
      if (randomised_level < 0) {
        randomised_level = 0;
      }
      if (randomised_decay_time < 0) {
        randomised_decay_time = 0;
      }
      // reset the level and time values
      level_[1] =  randomised_level ;
      time_[1] =  randomised_decay_time ;
    } else if (control & CONTROL_GATE_FALLING && sustain_point_) {
      start_value_ = value_;
      segment_ = sustain_point_;
      phase_ = 0;
    } else if (phase_ < phase_increment_) {
      start_value_ = level_[segment_ + 1];
      ++segment_;
      phase_ = 0;
      if (segment_ == loop_end_) {
        segment_ = loop_start_;
      }
    }

    bool done = segment_ == num_segments_;
    bool sustained = sustain_point_ && segment_ == sustain_point_ &&
        control & CONTROL_GATE;

    phase_increment_ =
        sustained || done ? 0 : lut_env_increments[time_[segment_] >> 8];

    int32_t a = start_value_;
    int32_t b = level_[segment_ + 1];
    uint16_t t = Interpolate824(
        lookup_table_table[LUT_ENV_LINEAR + shape_[segment_]], phase_);
    value_ = a + ((b - a) * (t >> 1) >> 15);
    phase_ += phase_increment_;
    *out++ = value_;
  }
}


}  // namespace peaks
