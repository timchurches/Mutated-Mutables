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

#include "peaks/processors.h"

#include <algorithm>

namespace peaks {

using namespace stmlib;
using namespace std;

#define REGISTER_PROCESSOR(ClassName) \
  { &Processors::ClassName ## Init, \
    &Processors::ClassName ## Process, \
    &Processors::ClassName ## Configure },

/* static */
const Processors::ProcessorCallbacks
Processors::callbacks_table_[PROCESSOR_FUNCTION_LAST] = {
  REGISTER_PROCESSOR(MultistageEnvelope)
  REGISTER_PROCESSOR(Lfo)
  REGISTER_PROCESSOR(Lfo)
  REGISTER_PROCESSOR(BassDrum)
  REGISTER_PROCESSOR(SnareDrum)
  REGISTER_PROCESSOR(HighHat)
  REGISTER_PROCESSOR(Cymbal)
  REGISTER_PROCESSOR(FmDrum)
  REGISTER_PROCESSOR(PulseShaper)
  REGISTER_PROCESSOR(PulseRandomizer)
  REGISTER_PROCESSOR(MiniSequencer)
  REGISTER_PROCESSOR(NumberStation)
  REGISTER_PROCESSOR(ByteBeats)
  REGISTER_PROCESSOR(DualAttackEnvelope)
  REGISTER_PROCESSOR(RepeatingAttackEnvelope)
  REGISTER_PROCESSOR(LoopingEnvelope)
  REGISTER_PROCESSOR(RandomisedEnvelope)
  REGISTER_PROCESSOR(BouncingBall)
  REGISTER_PROCESSOR(RandomisedBassDrum)
  REGISTER_PROCESSOR(RandomisedSnareDrum)
  REGISTER_PROCESSOR(TuringMachine)
  REGISTER_PROCESSOR(ModSequencer)
  REGISTER_PROCESSOR(FmLfo)
  REGISTER_PROCESSOR(FmLfo)
  REGISTER_PROCESSOR(WsmLfo)
  REGISTER_PROCESSOR(WsmLfo)
  REGISTER_PROCESSOR(Plo)
};

void Processors::Init(uint8_t index) {
  for (uint16_t i = 0; i < PROCESSOR_FUNCTION_LAST; ++i) {
    (this->*callbacks_table_[i].init_fn)();
  }

  bass_drum_.Init();
  snare_drum_.Init();
  high_hat_.Init();
  high_hat_.set_open(index == 1);
  cymbal_.Init();
  fm_drum_.Init();
  fm_drum_.set_sd_range(index == 1);
  bouncing_ball_.Init();
  lfo_.Init();
  envelope_.Init();
  pulse_shaper_.Init();
  pulse_randomizer_.Init();
  mini_sequencer_.Init();
  number_station_.Init();
  number_station_.set_voice(index == 1);
  bytebeats_.Init();
  turing_machine_.Init();
  dual_attack_envelope_.Init();
  looping_envelope_.Init();
  repeating_attack_envelope_.Init();
  randomised_envelope_.Init();
  randomised_bass_drum_.Init();
  randomised_snare_drum_.Init();
  mod_sequencer_.Init();
  fmlfo_.Init();
  wsmlfo_.Init();
  plo_.Init();

  control_mode_ = CONTROL_MODE_FULL;
  set_function(PROCESSOR_FUNCTION_ENVELOPE);
  std::fill(&parameter_[0], &parameter_[4], 32768);
}

/* extern */
Processors processors[2];

}  // namespace peaks
