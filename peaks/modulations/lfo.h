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
// LFO.

#ifndef PEAKS_MODULATIONS_LFO_H_
#define PEAKS_MODULATIONS_LFO_H_

#include "stmlib/stmlib.h"
#include "stmlib/algorithms/pattern_predictor.h"

#include "peaks/gate_processor.h"

namespace peaks {

struct FrequencyRatio {
  uint32_t p;
  uint32_t q;
};

enum LfoShape {
  LFO_SHAPE_SINE,
  LFO_SHAPE_TRIANGLE,
  LFO_SHAPE_SQUARE,
  LFO_SHAPE_STEPS,
  LFO_SHAPE_NOISE,
  LFO_SHAPE_LAST
};

enum WsmLfoShape {
  WSMLFO_SHAPE_FOLDED_SINE,
  WSMLFO_SHAPE_FOLDED_POWER_SINE,
  WSMLFO_SHAPE_OVERDRIVEN_SINE,
  WSMLFO_SHAPE_TRIANGLE,
  WSMLFO_SHAPE_SQUARE,
  WSMLFO_SHAPE_NOISE,
  WSMLFO_SHAPE_LAST
};

class Lfo {
 public:
  typedef int16_t (Lfo::*ComputeSampleFn)();
   
  Lfo() { }
  ~Lfo() { }
  
  void Init();
  void FillBuffer(InputBuffer* input_buffer, OutputBuffer* output_buffer);
  
  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      if (sync_) {
        set_shape_integer(parameter[0]);
        set_parameter(parameter[1] - 32768);
      } else {
        set_rate(parameter[0]);
        set_shape_parameter_preset(parameter[1]);
      }
      set_reset_phase(0);
      set_level(40960);
    } else {
      if (sync_) {
        set_level(parameter[0]);
        set_shape_integer(parameter[1]);
        set_parameter(parameter[2] - 32768);
        set_reset_phase(parameter[3] - 32768);
      } else {
        set_level(40960);
        set_rate(parameter[0]);
        set_shape_integer(parameter[1]);
        set_parameter(parameter[2] - 32768);
        set_reset_phase(parameter[3] - 32768);
      }
    }
  }
  
  inline void set_rate(uint16_t rate) {
    rate_ = rate;
  }
  
  inline void set_shape(LfoShape shape) {
    shape_ = shape;
  }

  inline void set_shape_integer(uint16_t value) {
    shape_ = static_cast<LfoShape>(value * LFO_SHAPE_LAST >> 16);
  }
  
  void set_shape_parameter_preset(uint16_t value);
  
  inline void set_parameter(int16_t parameter) {
    parameter_ = parameter;
  }
  
  inline void set_reset_phase(int16_t reset_phase) {
    reset_phase_ = static_cast<int32_t>(reset_phase) << 16;
  }
  
  inline void set_sync(bool sync) {
    if (!sync_ && sync) {
      pattern_predictor_.Init();
    }
    sync_ = sync;
  }
  
  inline void set_level(uint16_t level) {
    level_ = level >> 1;
  }
  
 private:
  int16_t ComputeSampleSine();
  int16_t ComputeSampleTriangle();
  int16_t ComputeSampleSquare();
  int16_t ComputeSampleSteps();
  int16_t ComputeSampleNoise();
   
  uint16_t rate_;
  LfoShape shape_;
  int16_t parameter_;
  int32_t reset_phase_;
  int32_t level_;

  bool sync_;
  uint32_t sync_counter_;
  stmlib::PatternPredictor<32, 8> pattern_predictor_;
  
  uint32_t phase_;
  uint32_t phase_increment_;
  
  uint32_t period_;
  uint32_t end_of_attack_;
  uint32_t attack_factor_;
  uint32_t decay_factor_;
  int16_t previous_parameter_;
  
  int32_t value_;
  int32_t next_value_;
  
  static ComputeSampleFn compute_sample_fn_table_[];

  DISALLOW_COPY_AND_ASSIGN(Lfo);
};

// Repeat for FM LFO
class FmLfo {
 public:
  typedef int16_t (FmLfo::*ComputeSampleFn)();
   
  FmLfo() { }
  ~FmLfo() { }
  
  void Init();
  void FillBuffer(InputBuffer* input_buffer, OutputBuffer* output_buffer);
  
  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      set_rate(parameter[0]);
      set_shape_parameter_preset(parameter[1]);
      set_reset_phase(0);
      set_level(65535);
    } else {
      set_level(65535);
      set_rate(parameter[0]);
      set_shape_parameter_preset(parameter[1]);
      set_fm_rate(parameter[2]);
      set_fm_depth(parameter[3]);
    }
  }
  
  inline void set_rate(uint16_t rate) {
    rate_ = rate;
  }

  inline void set_shape(LfoShape shape) {
    shape_ = shape;
  }

  inline void set_shape_integer(uint16_t value) {
    shape_ = static_cast<LfoShape>(value * LFO_SHAPE_LAST >> 16);
  }
  
  void set_shape_parameter_preset(uint16_t value);
  
  inline void set_fm_rate(uint16_t fm_rate) {
    fm_rate_ = fm_rate;
  }

  inline void set_fm_depth(uint16_t fm_depth) {
    if (fm_depth < 32768) {
      fm_depth_ = (32767 - fm_depth) << 1;
      fm_parameter_ = 0;
    } else {
      fm_depth_ = (fm_depth - 32768) << 1;
      fm_parameter_ = 16383;
    }
  }
  
  inline void set_parameter(int16_t parameter) {
    parameter_ = parameter;
  }
  
  inline void set_reset_phase(int16_t reset_phase) {
    reset_phase_ = static_cast<int32_t>(reset_phase) << 16;
  }
    
  inline void set_level(uint16_t level) {
    level_ = level >> 1;
  }

  inline void set_mod_type(bool mod_type) {
    random_mod_ = mod_type;
  }
  
    
 private:
  int16_t ComputeSampleSine();
  int16_t ComputeSampleTriangle();
  int16_t ComputeSampleSquare();
  int16_t ComputeSampleSteps();
  int16_t ComputeSampleNoise();
  int16_t ComputeModulation();
   
  uint16_t rate_;
  LfoShape shape_;
  int16_t parameter_;
  int32_t reset_phase_;
  int32_t level_;
  
  uint32_t phase_;
  uint32_t phase_increment_;

  uint16_t fm_rate_;
  uint16_t fm_depth_;
  int16_t fm_parameter_;
  int32_t fm_reset_phase_;

  uint32_t fm_phase_;
  uint32_t fm_phase_increment_;
  int16_t fm_delta_ ;

  bool random_mod_ ;
  
  uint32_t period_;
  uint32_t end_of_attack_;
  uint32_t attack_factor_;
  uint32_t decay_factor_;
  int16_t previous_parameter_;
  
  int32_t value_;
  int32_t next_value_;

  int32_t fm_value_;
  int32_t fm_next_value_;
  
  static ComputeSampleFn compute_sample_fn_table_[];

  DISALLOW_COPY_AND_ASSIGN(FmLfo);
};

//////////////////////////////
// And repeat again for waveshape modulated LFO
class WsmLfo {
 public:
  typedef int16_t (WsmLfo::*ComputeSampleFn)();
   
  WsmLfo() { }
  ~WsmLfo() { }
  
  void Init();
  void FillBuffer(InputBuffer* input_buffer, OutputBuffer* output_buffer);
  
  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      set_rate(parameter[0]);
      set_shape_parameter_preset(parameter[1]);
      set_reset_phase(0);
      set_level(65535);
    } else {
      set_rate(parameter[0]);
      set_shape_parameter_preset(parameter[1]);
      set_reset_phase(0);
      set_level(65535);
      set_wsm_rate(parameter[2]);
      set_wsm_depth(parameter[3]);
    }
  }
  
  inline void set_rate(uint16_t rate) {
    rate_ = rate;
  }

  inline void set_shape(WsmLfoShape shape) {
    shape_ = shape;
  }

  inline void set_shape_integer(uint16_t value) {
    shape_ = static_cast<WsmLfoShape>(value * WSMLFO_SHAPE_LAST >> 16);
  }
  
  void set_shape_parameter_preset(uint16_t value);
  
  inline void set_wsm_rate(uint16_t wsm_rate) {
    wsm_rate_ = wsm_rate;
  }

  inline void set_wsm_depth(uint16_t wsm_depth) {
    if (wsm_depth < 32768) {
      wsm_depth_ = (32767 - wsm_depth) << 1;
      wsm_parameter_ = 0;
    } else {
      wsm_depth_ = (wsm_depth - 32768) << 1;
      wsm_parameter_ = 16383;
    }
  }
  
  inline void set_parameter(int16_t parameter) {
    parameter_ = parameter;
  }
  
  inline void set_reset_phase(int16_t reset_phase) {
    reset_phase_ = static_cast<int32_t>(reset_phase) << 16;
  }
    
  inline void set_level(uint16_t level) {
    level_ = level >> 1;
  }
  
  inline void set_mod_type(bool mod_type) {
    random_mod_ = mod_type;
  }
    
 private:
  int16_t ComputeSampleFoldedSine();
  int16_t ComputeSampleFoldedPowerSine();
  int16_t ComputeSampleOverdrivenSine();
  int16_t ComputeSampleTriangle();
  int16_t ComputeSampleSquare();
  int16_t ComputeSampleNoise();
  int16_t ComputeModulation();
   
  uint16_t rate_;
  WsmLfoShape shape_;
  int16_t parameter_;
  int32_t reset_phase_;
  int32_t level_;
  
  uint16_t wsm_rate_;
  uint16_t wsm_depth_;
  int16_t wsm_parameter_;
  int32_t wsm_reset_phase_;

  uint32_t wsm_phase_;
  uint32_t wsm_phase_increment_;
  int16_t wsm_delta_ ;

  bool random_mod_ ;
  
  uint32_t phase_;
  uint32_t phase_increment_;
  
  uint32_t period_;
  uint32_t end_of_attack_;
  uint32_t attack_factor_;
  uint32_t decay_factor_;
  int16_t previous_parameter_;
  
  int32_t value_;
  int32_t next_value_;

  int32_t wsm_value_;
  int32_t wsm_next_value_;
    
  static ComputeSampleFn compute_sample_fn_table_[];

  DISALLOW_COPY_AND_ASSIGN(WsmLfo);
};

//////////////////////////////////
// And repeat again for audio-rate PLL oscillators
class Plo {
 public:
  typedef int16_t (Plo::*ComputeSampleFn)();
   
  Plo() { }
  ~Plo() { }
  
  void Init();
  void FillBuffer(InputBuffer* input_buffer, OutputBuffer* output_buffer);
  
  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      set_pitch_coefficient(parameter[0]);
      set_shape_parameter_preset(parameter[1]);
      set_level(65535);
    } else {
      set_pitch_coefficient(parameter[0]);
      set_shape_parameter_preset(parameter[1]);
      set_level(65535);
      set_wsm_rate(parameter[2]);
      set_wsm_depth(parameter[3]);
    }
  }
  
  inline void set_rate(uint16_t rate) {
    rate_ = rate;
  }

  inline void set_shape(WsmLfoShape shape) {
    shape_ = shape;
  }

  inline void set_shape_integer(uint16_t value) {
    shape_ = static_cast<WsmLfoShape>(value * WSMLFO_SHAPE_LAST >> 16);
  }
  
  void set_shape_parameter_preset(uint16_t value);

  void set_pitch_coefficient(uint16_t value);

  inline void set_sync(bool sync) {
    if (!sync_ && sync) {
      pattern_predictor_.Init();
    }
    sync_ = sync;
  }
  
  inline void set_wsm_rate(uint16_t wsm_rate) {
    wsm_rate_ = wsm_rate;
  }

  inline void set_wsm_depth(uint16_t wsm_depth) {
      wsm_depth_ = wsm_depth;
  }
  
  inline void set_parameter(int16_t parameter) {
    parameter_ = parameter;
  }
  
  // inline void set_reset_phase(int16_t reset_phase) {
  //   reset_phase_ = static_cast<int32_t>(reset_phase) << 16;
  // }
    
  inline void set_level(uint16_t level) {
    level_ = level >> 1;
  }
  
  // inline void set_mod_type(bool mod_type) {
  //   random_mod_ = mod_type;
  // }
    
 private:
  int16_t ComputeSampleFoldedSine();
  int16_t ComputeSampleFoldedPowerSine();
  int16_t ComputeSampleOverdrivenSine();
  int16_t ComputeSampleTriangle();
  int16_t ComputeSampleSquare();
  int16_t ComputeSampleNoise();
  int16_t ComputeModulationSine();
   
  uint16_t rate_;
  WsmLfoShape shape_;
  int16_t parameter_;
  int32_t reset_phase_;
  int32_t level_;
  
  uint16_t wsm_rate_;
  uint16_t wsm_depth_;
  int16_t wsm_parameter_;

  uint32_t wsm_phase_;
  uint32_t wsm_phase_increment_;

  bool sync_;
  uint32_t sync_counter_;
  stmlib::PatternPredictor<32, 8> pattern_predictor_;
  
  uint32_t phase_;
  uint32_t phase_increment_;
  
  uint32_t period_;
  uint32_t end_of_attack_;
  uint32_t attack_factor_;
  uint32_t decay_factor_;
  int16_t previous_parameter_;
  
  int32_t value_;
  int32_t next_value_;

  int32_t wsm_value_;
  int32_t wsm_next_value_;
  
  int8_t pitch_multiplier_ ;
  
  static ComputeSampleFn compute_sample_fn_table_[];

  DISALLOW_COPY_AND_ASSIGN(Plo);
};

}  // namespace peaks

#endif  // PEAKS_MODULATIONS_LFO_H_
