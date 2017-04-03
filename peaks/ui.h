// Copyright 2013 Olivier Gillet.
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
// Settings

#ifndef PEAKS_UI_H_
#define PEAKS_UI_H_

#include "stmlib/stmlib.h"

#include "stmlib/ui/event_queue.h"

#include "peaks/drivers/adc.h"
#include "peaks/drivers/gate_input.h"
#include "peaks/drivers/leds.h"
#include "peaks/drivers/switches.h"

#include "peaks/processors.h"

namespace peaks {

enum SwitchIndex {
  SWITCH_TWIN_MODE,
  SWITCH_FUNCTION,
  SWITCH_GATE_TRIG_1,
  SWITCH_GATE_TRIG_2
};

enum EditMode {
  EDIT_MODE_TWIN,
  EDIT_MODE_SPLIT,
  EDIT_MODE_FIRST,
  EDIT_MODE_SECOND,
  EDIT_MODE_LAST
};

enum Function {
  FUNCTION_ENVELOPE,
  FUNCTION_LFO,
  FUNCTION_TAP_LFO,
  FUNCTION_DRUM_GENERATOR,
  
  FUNCTION_DUAL_ATTACK_ENVELOPE,
  FUNCTION_REPEATING_ATTACK_ENVELOPE,
  FUNCTION_LOOPING_ENVELOPE,
  FUNCTION_RANDOMISED_ENVELOPE,
  FUNCTION_BOUNCING_BALL,

  FUNCTION_FMLFO,
  FUNCTION_RFMLFO,
  FUNCTION_WSMLFO,
  FUNCTION_RWSMLFO,
  FUNCTION_PLO,

  FUNCTION_MINI_SEQUENCER,
  FUNCTION_MOD_SEQUENCER,
  FUNCTION_PULSE_SHAPER,
  FUNCTION_PULSE_RANDOMIZER,
  FUNCTION_TURING_MACHINE,
  FUNCTION_BYTEBEATS,

  FUNCTION_FM_DRUM_GENERATOR,
  FUNCTION_RANDOMISED_DRUM_GENERATOR,
  FUNCTION_HIGH_HAT,

  FUNCTION_LAST,
  FUNCTION_FIRST_BASIC_FUNCTION = FUNCTION_ENVELOPE,
  FUNCTION_LAST_BASIC_FUNCTION = FUNCTION_DRUM_GENERATOR,

  FUNCTION_FIRST_EXTENDED_ENV_FUNCTION = FUNCTION_DUAL_ATTACK_ENVELOPE,
  FUNCTION_LAST_EXTENDED_ENV_FUNCTION = FUNCTION_BOUNCING_BALL,

  FUNCTION_FIRST_EXTENDED_LFO_FUNCTION = FUNCTION_FMLFO,
  FUNCTION_LAST_EXTENDED_LFO_FUNCTION = FUNCTION_PLO,

  FUNCTION_FIRST_EXTENDED_TAP_FUNCTION = FUNCTION_MINI_SEQUENCER,
  FUNCTION_LAST_EXTENDED_TAP_FUNCTION = FUNCTION_BYTEBEATS,

  FUNCTION_FIRST_EXTENDED_DRUM_FUNCTION = FUNCTION_FM_DRUM_GENERATOR,
  FUNCTION_LAST_EXTENDED_DRUM_FUNCTION = FUNCTION_HIGH_HAT,
};

struct Settings {
  uint8_t edit_mode;
  uint8_t function[2];
  uint8_t pot_value[8];
  bool snap_mode;
  uint8_t padding[4];
};

class CalibrationData;

class Ui {
 public:
  Ui() { }
  ~Ui() { }
  
  void Init(CalibrationData* calibration_data);
  void Poll();
  void PollPots();
  void DoEvents();
  void FlushEvents();
  
  void set_leds_brightness(int16_t channel_1, int16_t channel_2) {
    brightness_[0] = channel_1;
    brightness_[1] = channel_2;
  }
  
  inline uint8_t ReadPanelGateState() {
    uint8_t state = panel_gate_control_[0] ? INPUT_1_GATE : 0;
    state |= panel_gate_control_[1] ? INPUT_2_GATE : 0;
    if (!(panel_gate_state_ & INPUT_1_GATE) && (state & INPUT_1_GATE)) {
      state |= INPUT_1_RAISING;
    }
    if ((panel_gate_state_ & INPUT_1_GATE) && !(state & INPUT_1_GATE)) {
      state |= INPUT_1_FALLING;
    }
    if (!(panel_gate_state_ & INPUT_2_GATE) && (state & INPUT_2_GATE)) {
      state |= INPUT_2_RAISING;
    }
    if ((panel_gate_state_ & INPUT_2_GATE) && !(state & INPUT_2_GATE)) {
      state |= INPUT_2_FALLING;
    }
    panel_gate_state_ = state;
    return state;
  }
  
  inline bool calibrating() const { return calibrating_; }
  
 private:
  inline Function function() const {
    return edit_mode_ == EDIT_MODE_SECOND ? function_[1] : function_[0];
  }
  void LockPots();
  void OnSwitchPressed(const stmlib::Event& e);
  void OnSwitchReleased(const stmlib::Event& e);
  void OnPotChanged(const stmlib::Event& e);
  void RefreshLeds();
  
  void ChangeControlMode();
  void SetFunction(uint8_t index, Function f);
  void SaveState();

  uint16_t adc_value_[kNumAdcChannels];
  uint16_t adc_threshold_[kNumAdcChannels];
  uint32_t press_time_[kNumSwitches];
  bool panel_gate_control_[2];
  static const ProcessorFunction function_table_[FUNCTION_LAST][2];
  
  stmlib::EventQueue<32> queue_;
  
  Leds leds_;
  Switches switches_;
  Adc adc_;
  
  EditMode edit_mode_;
  Function function_[2];
  Settings settings_;
  uint16_t version_token_;
  
  int16_t brightness_[2];
  
  uint8_t panel_gate_state_;
  
  uint8_t double_press_counter_;
  uint8_t pot_value_[8];
  
  bool snap_mode_;
  bool snapped_[kNumAdcChannels];
  
  CalibrationData* calibration_data_;
  bool calibrating_;
  
  // store last used function on each page
  Function last_basic_function_[2];
  Function last_ext_env_function_[2];
  Function last_ext_lfo_function_[2];
  Function last_ext_tap_function_[2];
  Function last_ext_drum_function_[2];
  
  DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace peaks

#endif  // PEAKS_UI_H_
