Mutated-Mutables
================

Various enhancements, experiments and outright hacks of Mutable Instruments firmware code.

This repository is a copy of the Mutable Instruments GitHub repository at https://github.com/pichenettes/eurorack

So far, the only modified code is the Bees-in-Trees enhancements to Braids. The following features (and some anti-features) have all been implemented in Bees-in-Trees Version 3p, the source code for which is in the braids/ directory here. 

Bees-in-Trees v3p enhancements
=============================

* Instead of a single internal envelope, there are now two internal modulators, MOD1 and MOD2. The MOD1 and MOD2 menu settings allow each internal modulator to be turned off (OFF), put in envelope mode (ENV), or in LFO mode (LFO).
* Each internal modulator has a rate setting, RAT1 and RAT2 respectively, with ranges from 0 to 127. When in envelope mode, lower rate values produce shorter envelope segment (attack and decay) durations, and higher rate values produce longer envelope segment times. When in LFO mode, lower rate values produce slower LFO speeds (lower LFO frequency), and higher rate values produce faster speeds (higher frequencies), if the RINV setting is ON (the default). Turning RINV (rate inversion) off produces slower LFO speeds with higher RAT1 or RAT2 settings. That can be useful if you are controlling one envelope and one LFO with an external voltage - see below.
* Each internal modulator has three depth controls (M1>T, M1>C and M1>L, and M2>T, M2>C and M2>L), which set the depth of modulation for each of timbre, color and level (amplitude), between zero and 250. Each modulation destination receives a weighted sum of the modulator values. For example, if M1>T = 30 and the instantaneous valus of modulator 1 is, say, 100, and M2>T = 60 and the instantaneous value of modulator 2 is 200, then the timbre parameter will receive a value of (30 * 100) + (60 * 200) = 15000. Obviously the values for modulator 1 and modulator 2 change each time the envelopes/LFO are rendered, which is about 4,000 times a second (I think). The timbre and color potentiometers and/or the timbre and color CV inputs act as offsets - the weighted sum of modulator values is added to these offsets. The LOFF value is the equivalent offset for level. All values are clipped at 65335. 
* The ratio between the attack and decay segments of each envelope, or the rising part of the waveform and the falling part in LFO mode, can be set by menu choices labelled ⇑⇓1 and ⇑⇓2 (up and down arrows 1 and 2). These provide value choices ranging between 0.02 and 50.0. These values refer to the ratio between the duration of the attack segment of the attack-decay (AD) envelope, or the rising segment of the LFO waveform, and the duration of the decay segment of falling segment, respectively. Thus a value of 0.10 means that the attack part of the envelope is a tenth of the duration of the decay part. Likewise, the LFO waveform is asymmetrical, with the rising portion only one-tenth as long as the falling portion. Obviously a ratio setting of 1.00 produces a symmetrical waveform, and an envelope in which attack and decay are of equal duration. 
* Each of the two internal modulators has its own shape setting (SHP1 and SHP2). This sets the shape of the curve used for the attack and decay parts of the envelope, or the rising and falling parts of the envelope. The following curve shapes are available:
  * EXPO is an exponential curve, as used in the envelopes in the official Braids firmware;
  * LINR is a linear curve i.e. a straight line (and thus in LFO mode produces sawtooth, triangle or ramp waveforms, depending on the AD ratio setting);
  * WIGL is a wiggly curve, SINE is a sine wave (well, almost a sine wave - it re-uses an existing look-up table in the Braids code which is close to a sine wave);
  * SQRE is a square-ish curve - a square wave with rounded shoulders; 
  * BOWF is logarithmic curve with a flat top - it is actually an inverted version of the bowing envelope for the BOWD oscillator mode in Braids, hence the name; 
  * RNDE is the same as EXPO except that the target level for the top of the envelope or LFO waveform varies randomly on each envelope or LFO cycle; 
  * RANDL and RNDS are the same, except using linear and square-ish curves as described above; 
  * RNDM sets a fixed random level which is flat for the entire envelope segment or LFO half-wave - thus it acts like a traditional clocked sample-and-hold sampling a random voltage.
* The META menu setting has been replaced by an FMCV setting, which determines to what use the FM control voltage input is put. The available coices are: 
  * FREQ, which means the FM input does FM;
  * META, which is the same as META mode on in the official firmware - voltage on the FM input scans through the oscillator modes;
  * RATE, in which voltage on the FM input sets the duration of the envelope segments, or the frequency of the LFOs - thus providing voltage-controlled envelopes and/or LFOs, with the FM voltage affecting the duration/speed of both internal modulators;
  * RAT1 is the same except the FM voltage only affects modulator 1;
  * RAT2 is also the same but the FM voltage only affects modulator 2. Note that for the rate settings, the voltage on the FM input is added to the RAT1 and RAT2 values for each each modulator, thus a base LFO speed or envelope duration can be set using RAT1 and RAT2, and that can then be modified by voltage on the FM input.
  * BIT⇓ provides voltage control over bit crushing, as set by the BITS menu.
* LFO range has been enabled in the range (RANG) menu. This LFO range was always present in the Braids source code, but its selection was disabled. The LFO range seems to go down to about 1Hz or so - thus it doesn't make Braids into a proper LFO, but it is low enough for many LFO modulation duties. Braids certainly produces a lot more interesting LFO waveforms than your average voltage-controlled LFO, And of course the two internal LFOs can still modulate Braids while its range is set to LFO - thus you have two LFOs inside your LFO. 
* DRFT (VCO drift) is now a settable value, from zero (OFF) to 15. Fifteen produces noisy grunge, but settings of around 3 or 4 can sound good in some oscillator modes.
* TSRC, TDLY, OCTV, QNTZ, BITS, BRIG, CAL. and CV testing menu choices are unchanged and function exactly as they do in the official Braids firmware.

Anti-enhancements
=================
These changes were required in order to free up space in the firmware storage for the enhancements described above.

* A paschal oophorectomy has been performed: the Easter egg oscillator model has been removed and the ability to trigger Easter egg mode has been disabled. One could say that the code for it has been Pynchoned off… This was necessary to free up space for the enhancements described above. Sorry!
* The marquee feature has been removed.
* The QPSK oscillator model has been removed.
* The sample rate (RATE) setting has been removed. Braids is locked at the maximum 96K sample rate instead. That isn't a problem from a CPU load perspective, because the sample rate reduction method didn't mean that the Braids processor was working less hard, just that it was discarding every second sample, or all but every third sample, or all but every fourth sample etc. However, the BITS setting is still there and still works, so you can still do bit-depth reduction for lo-fi grungy sounds, if that is your thing, and DRFT can now be cranked up to levels 15 times greater than before, if you really wish to. 
* VCO tune flattening (FLAT) and signature waveshapping (SIGN) have been removed.

Discussion
==========
All these enhancements seem to work fine, but more extensive testing is required. They are intended for advanced users only, and assume thorough familiarity with the way Braids operates. The modulators can interact with each other, by design, in complex and interesting ways (their values are summed for each destination BTW, and the resulting sum is clipped - thus with the right offsets and modulation depths, you can achieve half-wave clipping effects etc), and this  substantially extends the range of sounds that can be coaxed out of a Braids, without using any other modules at all. Add a single external LFO or sequencer modulating the FM input and all sorts of really amazing things are possible. As such, Bees-in-Trees may be particularly useful in small systems where external modulation sources are few, but since it does not remove or subtract any important or commonly-used existing Braids features or capabilities, it may be useful even in large systems.

Feedback and suggestions are welcome and appreciated - please email tim.churches@gmail.com