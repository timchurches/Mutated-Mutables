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
// User interface.

#include "peaks/ui.h"

#include "stmlib/system/storage.h"

#include <algorithm>

#include "peaks/calibration_data.h"

namespace peaks {

using namespace std;
using namespace stmlib;

const uint16_t kAdcThresholdUnlocked = 1 << (16 - 10);  // 10 bits
const uint16_t kAdcThresholdLocked = 1 << (16 - 8);  // 8 bits
const int32_t kLongPressDuration = 600;

/* static */
const ProcessorFunction Ui::function_table_[FUNCTION_LAST][2] = {
  { PROCESSOR_FUNCTION_ENVELOPE, PROCESSOR_FUNCTION_ENVELOPE },
  { PROCESSOR_FUNCTION_LFO, PROCESSOR_FUNCTION_LFO },
  { PROCESSOR_FUNCTION_TAP_LFO, PROCESSOR_FUNCTION_TAP_LFO },
  { PROCESSOR_FUNCTION_BASS_DRUM, PROCESSOR_FUNCTION_SNARE_DRUM },
  
  { PROCESSOR_FUNCTION_DUAL_ATTACK_ENVELOPE, PROCESSOR_FUNCTION_DUAL_ATTACK_ENVELOPE },
  { PROCESSOR_FUNCTION_REPEATING_ATTACK_ENVELOPE, PROCESSOR_FUNCTION_REPEATING_ATTACK_ENVELOPE },
  { PROCESSOR_FUNCTION_LOOPING_ENVELOPE, PROCESSOR_FUNCTION_LOOPING_ENVELOPE },
  { PROCESSOR_FUNCTION_RANDOMISED_ENVELOPE, PROCESSOR_FUNCTION_RANDOMISED_ENVELOPE },
  { PROCESSOR_FUNCTION_BOUNCING_BALL, PROCESSOR_FUNCTION_BOUNCING_BALL },

  { PROCESSOR_FUNCTION_FMLFO, PROCESSOR_FUNCTION_FMLFO },
  { PROCESSOR_FUNCTION_RFMLFO, PROCESSOR_FUNCTION_RFMLFO },
  { PROCESSOR_FUNCTION_WSMLFO, PROCESSOR_FUNCTION_WSMLFO },
  { PROCESSOR_FUNCTION_RWSMLFO, PROCESSOR_FUNCTION_RWSMLFO },
  { PROCESSOR_FUNCTION_PLO, PROCESSOR_FUNCTION_PLO },
  
  { PROCESSOR_FUNCTION_MINI_SEQUENCER, PROCESSOR_FUNCTION_MINI_SEQUENCER },
  { PROCESSOR_FUNCTION_MOD_SEQUENCER, PROCESSOR_FUNCTION_MOD_SEQUENCER },
  { PROCESSOR_FUNCTION_PULSE_SHAPER, PROCESSOR_FUNCTION_PULSE_SHAPER },
  { PROCESSOR_FUNCTION_PULSE_RANDOMIZER, PROCESSOR_FUNCTION_PULSE_RANDOMIZER },
  { PROCESSOR_FUNCTION_TURING_MACHINE, PROCESSOR_FUNCTION_TURING_MACHINE },
  { PROCESSOR_FUNCTION_BYTEBEATS, PROCESSOR_FUNCTION_BYTEBEATS },

  { PROCESSOR_FUNCTION_FM_DRUM, PROCESSOR_FUNCTION_FM_DRUM },
  { PROCESSOR_FUNCTION_CYMBAL, PROCESSOR_FUNCTION_CYMBAL },
  { PROCESSOR_FUNCTION_RANDOMISED_BASS_DRUM, PROCESSOR_FUNCTION_RANDOMISED_SNARE_DRUM },
  { PROCESSOR_FUNCTION_HIGH_HAT, PROCESSOR_FUNCTION_HIGH_HAT },
};

Storage<0x8020000, 16> storage;

void Ui::Init(CalibrationData* calibration_data) {
  calibration_data_ = calibration_data;
  leds_.Init();
  switches_.Init();
  adc_.Init();
  system_clock.Tick();

  fill(&adc_lp_[0], &adc_lp_[kNumAdcChannels], 0);
  fill(&adc_value_[0], &adc_value_[kNumAdcChannels], 0);
  fill(&adc_threshold_[0], &adc_threshold_[kNumAdcChannels], 0);
  fill(&snapped_[0], &snapped_[kNumAdcChannels], false);
  panel_gate_state_ = 0;
  
  calibrating_ = switches_.pressed_immediate(1);
  
  if (!storage.ParsimoniousLoad(&settings_, &version_token_)) {
    edit_mode_ = EDIT_MODE_TWIN;
    function_[0] = FUNCTION_ENVELOPE;
    function_[1] = FUNCTION_ENVELOPE;
    settings_.snap_mode = false;
  } else {
    edit_mode_ = static_cast<EditMode>(settings_.edit_mode);
    function_[0] = static_cast<Function>(settings_.function[0]);
    function_[1] = static_cast<Function>(settings_.function[1]);
    copy(&settings_.pot_value[0], &settings_.pot_value[8], &pot_value_[0]);
    
    if (edit_mode_ == EDIT_MODE_FIRST || edit_mode_ == EDIT_MODE_SECOND) {
      LockPots();
      for (uint8_t i = 0; i < 4; ++i) {
        processors[0].set_parameter(
            i,
            static_cast<uint16_t>(pot_value_[i]) << 8);
        processors[1].set_parameter(
            i,
            static_cast<uint16_t>(pot_value_[i + 4]) << 8);
      }
    }
  }
  
  if (switches_.pressed_immediate(SWITCH_TWIN_MODE)) {
    settings_.snap_mode = !settings_.snap_mode;
    SaveState();
  }
  
  ChangeControlMode();
  SetFunction(0, function_[0]);
  SetFunction(1, function_[1]);
  double_press_counter_ = 0;
  
  last_basic_function_[0] = last_basic_function_[1] = FUNCTION_FIRST_BASIC_FUNCTION;
  last_ext_env_function_[0] = last_ext_env_function_[1] = FUNCTION_FIRST_EXTENDED_ENV_FUNCTION;
  last_ext_lfo_function_[0] = last_ext_lfo_function_[1] = FUNCTION_FIRST_EXTENDED_LFO_FUNCTION;
  last_ext_tap_function_[0] = last_ext_tap_function_[1] = FUNCTION_FIRST_EXTENDED_TAP_FUNCTION;
  last_ext_drum_function_[0] = last_ext_drum_function_[1] = FUNCTION_FIRST_EXTENDED_DRUM_FUNCTION;

}

void Ui::LockPots() {
  fill(
      &adc_threshold_[0],
      &adc_threshold_[kNumAdcChannels],
      kAdcThresholdLocked);
  fill(&snapped_[0], &snapped_[kNumAdcChannels], false);
}

void Ui::SaveState() {
  settings_.edit_mode = edit_mode_;
  settings_.function[0] = function_[0];
  settings_.function[1] = function_[1];
  copy(&pot_value_[0], &pot_value_[8], &settings_.pot_value[0]);
  settings_.padding[0] = 0;
  settings_.padding[1] = 0;
  settings_.padding[2] = 0;
  settings_.padding[3] = 0;
  storage.ParsimoniousSave(settings_, &version_token_);
}

inline void Ui::RefreshLeds() {
    if (calibrating_) {
    leds_.set_pattern(0xf);
    leds_.set_twin_mode(true);
    leds_.set_levels(0, 0);
    return;
  }
  uint8_t flash = (system_clock.milliseconds() >> 7) & 7;
  switch (edit_mode_) {
    case EDIT_MODE_FIRST:
      leds_.set_twin_mode(flash == 1);
      break;
    case EDIT_MODE_SECOND:
      leds_.set_twin_mode(flash == 1 || flash == 3);
      break;
    default:
      leds_.set_twin_mode(edit_mode_ & 1);
      break;
  }
  if ((system_clock.milliseconds() & 256) &&
      function() > FUNCTION_LAST_BASIC_FUNCTION) {
      switch (function()) {
        // x = on constantly, X = blinking, 0 = off
        // extended ENV functions
        case FUNCTION_DUAL_ATTACK_ENVELOPE:
        case FUNCTION_REPEATING_ATTACK_ENVELOPE:
        case FUNCTION_LOOPING_ENVELOPE:
        case FUNCTION_RANDOMISED_ENVELOPE:
        case FUNCTION_BOUNCING_BALL:
          leds_.set_pattern(1); // top LED-> x 0 X X
          break;
        // extended  LFO functions
        case FUNCTION_FMLFO:
        case FUNCTION_RFMLFO:
        case FUNCTION_WSMLFO:
        case FUNCTION_RWSMLFO:
        case FUNCTION_PLO:
          leds_.set_pattern(2); // top LED-> 0 x 0 X
          break;
        // extended TAP functions
        case FUNCTION_MINI_SEQUENCER:
        case FUNCTION_MOD_SEQUENCER:
        case FUNCTION_PULSE_SHAPER:
        case FUNCTION_PULSE_RANDOMIZER:
        case FUNCTION_TURING_MACHINE:
        case FUNCTION_BYTEBEATS:
          leds_.set_pattern(4); // top LED-> X X x X
          break;
        // extended DRUM functions
        case FUNCTION_HIGH_HAT:
		case FUNCTION_CYMBAL:
        case FUNCTION_FM_DRUM_GENERATOR:
        case FUNCTION_RANDOMISED_DRUM_GENERATOR:
          leds_.set_pattern(8); // top LED-> 0 0 X x
          break;
        // the remainder
        default:
          leds_.set_function(4);
          break;
      }
  } else {
    switch (function()) {
      // x = on constantly, X = blinking, 0 = off
      // extended ENV functions
      case FUNCTION_DUAL_ATTACK_ENVELOPE:
        leds_.set_pattern(3); // top LED-> x X 0 0 
        break;
      case FUNCTION_REPEATING_ATTACK_ENVELOPE:
        leds_.set_pattern(5); // top LED-> x 0 X 0
        break;
      case FUNCTION_LOOPING_ENVELOPE:
        leds_.set_pattern(9); // top LED-> x 0 0 X
        break;
      case FUNCTION_RANDOMISED_ENVELOPE:
        leds_.set_pattern(7); // top LED-> x X X 0
        break;
      case FUNCTION_BOUNCING_BALL:
        leds_.set_pattern(13); // top LED-> x 0 X X
        break;
      // extended  LFO functions
      case FUNCTION_FMLFO:
        leds_.set_pattern(6); // top LED-> 0 x X 0
        break;
      case FUNCTION_RFMLFO:
        leds_.set_pattern(7); // top LED-> X x X 0
        break;
      case FUNCTION_WSMLFO:
        leds_.set_pattern(10); // top LED-> 0 x 0 X
        break;
      case FUNCTION_RWSMLFO:
        leds_.set_pattern(11); // top LED-> X x 0 X
        break;
      case FUNCTION_PLO:
        leds_.set_pattern(14); // top LED-> 0 x X X
        break;
      // extended TAP functions
      case FUNCTION_MINI_SEQUENCER:
        leds_.set_pattern(5); // top LED-> X 0 x 0
        break;
      case FUNCTION_MOD_SEQUENCER:
        leds_.set_pattern(6); // top LED-> 0 X x 0
        break;
      case FUNCTION_PULSE_SHAPER:
        leds_.set_pattern(12); // top LED-> 0 0 x X
        break;
      case FUNCTION_PULSE_RANDOMIZER:
        leds_.set_pattern(14); // top LED-> 0 X x X
        break;
      case FUNCTION_TURING_MACHINE:
        leds_.set_pattern(13); // top LED-> X 0 x X
        break;
      case FUNCTION_BYTEBEATS:
        leds_.set_pattern(15); // top LED-> X X x X
        break;
      // extended DRUM functions
      case FUNCTION_HIGH_HAT:
        leds_.set_pattern(12); // top LED-> 0 0 X x
        break;
      case FUNCTION_CYMBAL:
        leds_.set_pattern(10); // top LED-> 0 X 0 x
        break;
      case FUNCTION_FM_DRUM_GENERATOR:
        leds_.set_pattern(9); // top LED-> X 0 0 x
        break;
      case FUNCTION_RANDOMISED_DRUM_GENERATOR:
        leds_.set_pattern(11); // top LED-> X X 0 x
        break;
      // the remainder
      default:
        leds_.set_function(function() & 3);
        break;
    }
  }
  
  uint8_t b[2];
  for (uint8_t i = 0; i < 2; ++i) {
    switch (function_[i]) {
      case FUNCTION_DRUM_GENERATOR:
      case FUNCTION_FM_DRUM_GENERATOR:
      case FUNCTION_RANDOMISED_DRUM_GENERATOR:
      case FUNCTION_HIGH_HAT:
        b[i] = abs(brightness_[i]) >> 8;
        b[i] = b[i] > 255 ? 255 : b[i];
        break;
      case FUNCTION_LFO:
      case FUNCTION_TAP_LFO:
      case FUNCTION_FMLFO:
      case FUNCTION_RFMLFO:
      case FUNCTION_WSMLFO:
      case FUNCTION_RWSMLFO:
      case FUNCTION_PLO:
      case FUNCTION_MINI_SEQUENCER:
      case FUNCTION_MOD_SEQUENCER:
        {
          int32_t brightness = int32_t(brightness_[i]) * 409 >> 8;
          brightness += 32768;
          brightness >>= 8;
          CONSTRAIN(brightness, 0, 255);
          b[i] = brightness;
        }
        break;
      case FUNCTION_TURING_MACHINE:
        b[i] = static_cast<uint16_t>(brightness_[i]) >> 5;
        break;
      default:
        b[i] = brightness_[i] >> 7;
        break;
    }
  }
  
  if (processors[0].function() == PROCESSOR_FUNCTION_NUMBER_STATION) {
    leds_.set_pattern(
        processors[0].number_station().digit() ^ \
        processors[1].number_station().digit());
    b[0] = processors[0].number_station().gate() ? 255 : 0;
    b[1] = processors[1].number_station().gate() ? 255 : 0;
  }
  
  leds_.set_levels(b[0], b[1]);
}

void Ui::PollPots() {
  for (uint8_t i = 0; i < kNumAdcChannels; ++i) {
    adc_lp_[i] = (int32_t(adc_.value(i)) + adc_lp_[i] * 7) >> 3;
    int32_t value = adc_lp_[i];
    int32_t current_value = adc_value_[i];
    if (value >= current_value + adc_threshold_[i] ||
        value <= current_value - adc_threshold_[i] ||
        !adc_threshold_[i]) {
      Event e;
      e.control_id = i;
      e.data = value;
      OnPotChanged(e);
      adc_value_[i] = value;
      adc_threshold_[i] = kAdcThresholdUnlocked;
    }
  }
}

void Ui::Poll() {
  system_clock.Tick();
  switches_.Debounce();
  for (uint8_t i = 0; i < kNumSwitches; ++i) {
    if (switches_.just_pressed(i)) {
      queue_.AddEvent(CONTROL_SWITCH, i, 0);
      press_time_[i] = system_clock.milliseconds();
    }
    if (switches_.pressed(i) && press_time_[i] != 0 && i < SWITCH_GATE_TRIG_1) {
      int32_t pressed_time = system_clock.milliseconds() - press_time_[i];
      if (pressed_time > kLongPressDuration) {
        if (switches_.pressed(1 - i)) {
          ++double_press_counter_;
          press_time_[0] = press_time_[1] = 0;
          if (double_press_counter_ == 3) {
            double_press_counter_ = 0;
            processors[0].set_function(PROCESSOR_FUNCTION_NUMBER_STATION);
            processors[1].set_function(PROCESSOR_FUNCTION_NUMBER_STATION);
          }
        } else {
          queue_.AddEvent(CONTROL_SWITCH, i, pressed_time);
          press_time_[i] = 0;  // Inhibit next release event
        }
      }
    }
    if (switches_.released(i) && press_time_[i] != 0) {
      queue_.AddEvent(
          CONTROL_SWITCH,
          i,
          system_clock.milliseconds() - press_time_[i] + 1);
    }
  }
  
  RefreshLeds();
  leds_.Write();
}

void Ui::FlushEvents() {
  queue_.Flush();
}

void Ui::OnSwitchPressed(const Event& e) {
  switch (e.control_id) {
    case SWITCH_TWIN_MODE:
      break;
      
    case SWITCH_FUNCTION:
      break;
      
    case SWITCH_GATE_TRIG_1:
      panel_gate_control_[0] = true;
      break;
    
    case SWITCH_GATE_TRIG_2:
      panel_gate_control_[1] = true;
      break;
  }
}

void Ui::ChangeControlMode() {
  uint16_t parameters[4];
  for (int i = 0; i < 4; ++i) {
    parameters[i] = adc_value_[i];
  }
  if (edit_mode_ == EDIT_MODE_SPLIT) {
    processors[0].CopyParameters(&parameters[0], 2);
    processors[1].CopyParameters(&parameters[2], 2);
    processors[0].set_control_mode(CONTROL_MODE_HALF);
    processors[1].set_control_mode(CONTROL_MODE_HALF);
  } else if (edit_mode_ == EDIT_MODE_TWIN) {
    processors[0].CopyParameters(&parameters[0], 4);
    processors[1].CopyParameters(&parameters[0], 4);
    processors[0].set_control_mode(CONTROL_MODE_FULL);
    processors[1].set_control_mode(CONTROL_MODE_FULL);
  } else {
    processors[0].set_control_mode(CONTROL_MODE_FULL);
    processors[1].set_control_mode(CONTROL_MODE_FULL);
  }
}

void Ui::SetFunction(uint8_t index, Function f) {
  if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
    function_[0] = function_[1] = f;
    processors[0].set_function(function_table_[f][0]);
    processors[1].set_function(function_table_[f][1]);
  } else {
    function_[index] = f;
    processors[index].set_function(function_table_[f][index]);
  }
  // store current function for current function page
  switch (f) {
    case FUNCTION_ENVELOPE:
    case FUNCTION_LFO:
    case FUNCTION_TAP_LFO:
    case FUNCTION_DRUM_GENERATOR:
      if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
        last_basic_function_[0] = last_basic_function_[1] = f;
      } else {
        last_basic_function_[index] = f ;
      }
      break;
    case FUNCTION_DUAL_ATTACK_ENVELOPE:
    case FUNCTION_REPEATING_ATTACK_ENVELOPE:
    case FUNCTION_LOOPING_ENVELOPE:
    case FUNCTION_RANDOMISED_ENVELOPE:
    case FUNCTION_BOUNCING_BALL:
      if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
        last_ext_env_function_[0] = last_ext_env_function_[1] = f;
      } else {
        last_ext_env_function_[index] = f ;
      }
      break;
    case FUNCTION_FMLFO:
    case FUNCTION_RFMLFO:
    case FUNCTION_WSMLFO:
    case FUNCTION_RWSMLFO:
    case FUNCTION_PLO:
      if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
        last_ext_lfo_function_[0] = last_ext_lfo_function_[1] = f;
      } else {
        last_ext_lfo_function_[index] = f ;
      }
      break;
    case FUNCTION_MINI_SEQUENCER:
    case FUNCTION_MOD_SEQUENCER:
    case FUNCTION_PULSE_SHAPER:
    case FUNCTION_PULSE_RANDOMIZER:
    case FUNCTION_TURING_MACHINE:
    case FUNCTION_BYTEBEATS:
      if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
        last_ext_tap_function_[0] = last_ext_tap_function_[1] = f;
      } else {
        last_ext_tap_function_[index] = f ;
      }
      break;
    case FUNCTION_FM_DRUM_GENERATOR:
    case FUNCTION_HIGH_HAT:
    case FUNCTION_CYMBAL:
    case FUNCTION_RANDOMISED_DRUM_GENERATOR:
      if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
        last_ext_drum_function_[0] = last_ext_drum_function_[1] = f;
      } else {
        last_ext_drum_function_[index] = f ;
      }
      break;
    default:
      break;
  }
}

void Ui::OnSwitchReleased(const Event& e) {
	  if (calibrating_) {
    if (e.control_id == SWITCH_TWIN_MODE) {
      // Save calibration.
      calibration_data_->Save();

      // Reset all settings to defaults.
      edit_mode_ = EDIT_MODE_TWIN;
      function_[0] = FUNCTION_ENVELOPE;
      function_[1] = FUNCTION_ENVELOPE;
      settings_.snap_mode = false;
      
      SaveState();
      ChangeControlMode();
      SetFunction(0, function_[0]);
      SetFunction(1, function_[1]);

      // Done with calibration.
      calibrating_ = false;
    }
    return;
  }
  switch (e.control_id) {
    case SWITCH_TWIN_MODE:
      if (e.data > kLongPressDuration) {
        edit_mode_ = static_cast<EditMode>(
            (edit_mode_ + EDIT_MODE_FIRST) % EDIT_MODE_LAST);
        function_[0] = function_[1];
        processors[0].set_function(function_table_[function_[0]][0]);
        processors[1].set_function(function_table_[function_[0]][1]);
        LockPots();
      } else {
        if (edit_mode_ <= EDIT_MODE_SPLIT) {
          edit_mode_ = static_cast<EditMode>(EDIT_MODE_SPLIT - edit_mode_);
        } else {
          edit_mode_ = static_cast<EditMode>(EDIT_MODE_SECOND - (edit_mode_ & 1));
          LockPots();
        }
      }
      
      ChangeControlMode();
      SaveState();
      break;
      
    case SWITCH_FUNCTION:
      {
        Function f = function();
        if (e.data > kLongPressDuration) {
          if (f < FUNCTION_FIRST_EXTENDED_ENV_FUNCTION) {
            if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
              f = static_cast<Function>(last_ext_env_function_[0]);            
            } else {
              f = static_cast<Function>(last_ext_env_function_[edit_mode_ - EDIT_MODE_FIRST]);
            }
          } else if (f < FUNCTION_FIRST_EXTENDED_LFO_FUNCTION) {
            if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
              f = static_cast<Function>(last_ext_lfo_function_[0]);            
            } else {
              f = static_cast<Function>(last_ext_lfo_function_[edit_mode_ - EDIT_MODE_FIRST]);
            }
          } else if (f < FUNCTION_FIRST_EXTENDED_TAP_FUNCTION) {
            if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
              f = static_cast<Function>(last_ext_tap_function_[0]);            
            } else {
              f = static_cast<Function>(last_ext_tap_function_[edit_mode_ - EDIT_MODE_FIRST]);
            }
          } else if (f < FUNCTION_FIRST_EXTENDED_DRUM_FUNCTION) {
            if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
              f = static_cast<Function>(last_ext_drum_function_[0]);            
            } else {
              f = static_cast<Function>(last_ext_drum_function_[edit_mode_ - EDIT_MODE_FIRST]);
            }
          } else {
            if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
              f = static_cast<Function>(last_basic_function_[0]);            
            } else {
              f = static_cast<Function>(last_basic_function_[edit_mode_ - EDIT_MODE_FIRST]);
            }
          }
        } else {
          if (f <= FUNCTION_LAST_BASIC_FUNCTION) {
            f = static_cast<Function>((f + 1) & 3);
          } else if (f <= FUNCTION_LAST_EXTENDED_ENV_FUNCTION){
            f = static_cast<Function>(f + 1);
            if (f > FUNCTION_LAST_EXTENDED_ENV_FUNCTION) {
              f = static_cast<Function>(FUNCTION_FIRST_EXTENDED_ENV_FUNCTION);
            }
          } else if (f <= FUNCTION_LAST_EXTENDED_LFO_FUNCTION){
            f = static_cast<Function>(f + 1);
            if (f > FUNCTION_LAST_EXTENDED_LFO_FUNCTION) {
              f = static_cast<Function>(FUNCTION_FIRST_EXTENDED_LFO_FUNCTION);
            }
          } else if (f <= FUNCTION_LAST_EXTENDED_TAP_FUNCTION){
            f = static_cast<Function>(f + 1);
            if (f > FUNCTION_LAST_EXTENDED_TAP_FUNCTION) {
              f = static_cast<Function>(FUNCTION_FIRST_EXTENDED_TAP_FUNCTION);
            }
          } else if (f <= FUNCTION_LAST_EXTENDED_DRUM_FUNCTION){
            f = static_cast<Function>(f + 1);
            if (f > FUNCTION_LAST_EXTENDED_DRUM_FUNCTION) {
              f = static_cast<Function>(FUNCTION_FIRST_EXTENDED_DRUM_FUNCTION);
            }
          }
        }
        SetFunction(edit_mode_ - EDIT_MODE_FIRST, f);
        SaveState();
      }
      break;
      
    case SWITCH_GATE_TRIG_1:
      panel_gate_control_[0] = false;
      break;

    case SWITCH_GATE_TRIG_2:
      panel_gate_control_[1] = false;
      break;
  }
}

void Ui::OnPotChanged(const Event& e) {
  if (calibrating_) {
    pot_value_[e.control_id] = e.data >> 8;
    for (uint8_t i = 0; i < 2; ++i) {
      int32_t coarse = pot_value_[i * 2];
      int32_t fine = pot_value_[i * 2 + 1];
      int32_t offset = ((coarse - 128) << 3) + ((fine - 128) >> 1);
      calibration_data_->set_dac_offset(i, -offset);
    }
    return;
  }

  switch (edit_mode_) {
    case EDIT_MODE_TWIN:
      processors[0].set_parameter(e.control_id, e.data);
      processors[1].set_parameter(e.control_id, e.data);
      pot_value_[e.control_id] = e.data >> 8;
      break;
    case EDIT_MODE_SPLIT:
      if (e.control_id < 2) {
        processors[0].set_parameter(e.control_id, e.data);
      } else {
        processors[1].set_parameter(e.control_id - 2, e.data);
      }
      pot_value_[e.control_id] = e.data >> 8;
      break;
    case EDIT_MODE_FIRST:
    case EDIT_MODE_SECOND:
      {
        uint8_t index = e.control_id + (edit_mode_ - EDIT_MODE_FIRST) * 4;
        Processors* p = &processors[edit_mode_ - EDIT_MODE_FIRST];
        
        int16_t delta = static_cast<int16_t>(pot_value_[index]) - \
            static_cast<int16_t>(e.data >> 8);
        if (delta < 0) {
          delta = -delta;
        }
        
        if (!settings_.snap_mode || snapped_[e.control_id] || delta <= 2) {
          p->set_parameter(e.control_id, e.data);
          pot_value_[index] = e.data >> 8;
          snapped_[e.control_id] = true;
        }
      }
      break;
    case EDIT_MODE_LAST:
      break;
  }
}

void Ui::DoEvents() {
  while (queue_.available()) {
    Event e = queue_.PullEvent();
    if (e.control_type == CONTROL_SWITCH) {
      if (e.data == 0) {
        OnSwitchPressed(e);
      } else {
        OnSwitchReleased(e);
      }
    } else if (e.control_type == CONTROL_POT) {
      OnPotChanged(e);
    }
  }
  if (queue_.idle_time() > 1000) {
    queue_.Touch();
  }
}

}  // namespace peaks
