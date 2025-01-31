### November 12, 2024 - V1.9.9.

- Fix a couple compiler warnings.
- Update all dependencies to latest.
- Update all supported hosts (for testing) to latest.
- Add new host test file for FLStudio + CLAP, seems OK now.
- Breaking change: since JUCE 8 is minimum Windows 10, now so is Firefly!
- Replace homebrew fft with juce version, should speed up filter graph rendering a bit.

### November 7, 2024 - V1.9.8.

- Global LFO now reacts to phase adjustment when in snap-to-host-time mode.
- Bugfix: per-voice LFO random generators would not be plotted when no voice is active.
- Bugfix: global LFO would not react to single-cycle mode when snapped to project time.
- Bugfix: global LFO would get stuck in one-shot mode preventing to switch back to repeating mode.
- Add MSEG LFOs (see manual).
* Reacts to phase adjustment, stair-stepping and smoothing, but NOT phase/amp skewing (don't have the ui space for it).
* Otherwise equal to MSEG Envelopes w.r.t. GUI and automation/modulation except for sustain point.

### November 3, 2024 - V1.9.7.

- Bugfix: graph would not display modulation correctly when releasing envelope before the sustain point.
- Add MSEG Envelope support (it's hidden inside envelope mode) (see manual).
* Visual editor, snap-to-grid, max 16 segments, single sustain point.
* Optional resizable pop-up editor for more precise control.
* All y points and slopes fully automatable and per-voice-start modulatable.
* All width, y points and slopes with fully host-integrated context menu like any other param.
* When horizontal snapping is off, all segment widths automatable and per-voice-start modulatable.

### October 23, 2024 - V1.9.6.

- Fixed a couple of outdated parameter descriptions.
- Show parameter descriptions (from reference document) in tooltips.
- Bugfix: CV-to-envelope modulation did not take the voice start sample position within the block into account.
- Optionally allow global LFO's to sync to DAW project time (does NOT work with rate modulation, i.e. causes full phase reset).

### October 9, 2024 - V1.9.5.

- Added arpeggiator (see manual).
- UI: CV matrix now reacts to MIDI input.
- UI: CV matrix on-note menu is now split out into submenus for GLFO/MIDI etc.
- Bugfix: on-note-MIDI selection was off by 1 (e.g. on-note CC4 got you CC3).
- Bugfix: global-in and master parameters would not rerender the main graph when modulation is active.

### September 25, 2024 - V1.9.4.

- Added dark themes.

### September 21, 2024 - V1.9.3.

- Added free-running smooth noise as a new LFO type. Note - consecutive cycles are not smooth by themselves so you still need to use the LFO filter to get a real smooth signal.
- Bugfix in generating random numbers for the per-voice-random cv source (they weren't all that random).
- Breaking change: lfo noise generators become phase-based sampling functions to prevent drift.
* This causes small and possibly audible differences for the (free-running-) static and smooth lfos, sorry, but was needed to keep engine and ui in check.
* On the upside, they now correctly respond to phase offset.
- LFO graphs now respond to per-voice-seeded random generators.
- LFO and CV graphs now respond to free-running random generators.
- Envelope and CV graphs now respond to retriggered/multi-triggered envelopes.
- CV graphs now respond to on-note-global-lfo, on-voice-random-seed, MIDI key and velocity.
* Not! to MICI CC and CLAP modulation signals, maybe later.
- CV graphs are now animated in all cases, replaced phase indicators (because they were wrong) by a continuous moving signal.

### September 11, 2024 - V1.9.2.

- Bugfix: modulated params would be slow to visually react to user input.
- Allow to use the DSF function for distortion (basically a waveshaper with more knobs to play with).

### September 5, 2024 - V1.9.1.

- Add full realtime visual modulation for the graphs (so they show what the audio engine is actually doing if you f.e. set LFO to filter freq).
- Also added the option to back down on realtime repaint (nothing / params only / graphs) since this is quite cpu expensive.

### August 29, 2024 - V1.9.0.

- Load/save/init/clear patch split out to separate buttons again.
- Move global unison from global to voice section.
- Make global unison osc detune, osc phase and stereo spread modulatable parameters.
- Bugfix: only up to 20 routes were processed for cv->audio matrix.
- Renamed master section to global, made a new master section.
- Theme selector moved to new master section.
- Smoothing parameters, factory preset and tuning mode moved to new master section.
- Smoothing parameters, factory preset and tuning mode are now saved per-instance not per-patch (breaking change, old values are not preserved).

### August 24, 2024 - V1.8.9.

- Major UI overhaul, again.
- Output CPU/Gain is now meters instead of text.
- Dropped the existing themes and replaced by 2 new ones.
- Merged some gui sections to make the thing look less crowded.
- Moved all global matrices to the top, voice matrices to the bottom.
- Some cryptic combobox items now have useful tooltips (e.g. filter "LS" -> "Low Shelf").

### August 21, 2024 - V1.8.8.

- Add voice drain indicator to monitor.
- Moved load/save/init/clear patch into a single menu.
- Add Microtuning (MTS-ESP) support (experimental, see manual).
- Moved smoothing parameters from Master In to new Smoothing module.
- Fixed bug w.r.t. discarding dropdown items (by clicking outside the menu).
- Bugfix + breaking change: portamento would glide from last note instead of current position.
- Conversion code will fix existing patches, but Renoise automation data may be broken (that's why it's still not supported).

### August 21, 2024 - V1.8.7.

- Speed up rendering of modulation visualizers for graphs.
- Add modulation visualizers for parameters.

### August 20, 2024 - V1.8.6.

- Small changes and bugfixes to graphs.
- Add visual modulation indicators to graphs (small dots that follow the envelopes and lfo's. No visuals for parameter modulation yet).

### August 14, 2024 - V1.8.5.

- Add basic multiband EQ.
- Add another preset + demo tune.
- Make binaries run on Ubuntu 18 (was 22).
- Make binaries run on macOS 10.15 (was 14.x).
- Add osc phase as a modulatable parameter (PM).
- Make osc unison phase a modulatable parameter.

### July 17, 2024 - V1.8.3.

- Add drag-and-drop to mod matrix support.
- You can now populate the matrices by dragging module headers and parameter labels onto them. See manual for details.

### July 10, 2024 - V1.8.2.

- Add some presets + demo tune.

Add some random goodness!
Just some non-deterministic mod sources which result in different values
for each voice (rather than for-each-sample-start-position, as is the case with
sampling (On Note) from global random LFO). This is mostly useful to
add some more subtle "live-like" touch to a patch but of course it's
a mod source like anything else to potentially go crazy with.

- New modulation sources: on-note-random (inside the on-note menu, there's 3 of them).
These are fixed values at voice start but differ from on-note-global-random-lfo
in that they are re-drawn for each voice. So this gets you a random value as mod source
per-voice. Useful for chords.

- New per-voice LFO types: static 2, smooth noise 2, free running static noise 2.
These are conceptually similar to the existing smooth/static/free-static,
but instead of a fixed user-supplied seed value, they draw the seed value
from the above mentioned on-voice-random source. This gets you an entire new
random stream at the start of each voice. For hosts that allow you to play the same
note twice at the exact same sample position (like renoise), this allows to render
multiple completely different f.e. "C4" notes with the same start position.
But again, mostly useful for chords.

### July 6, 2024 - V1.8.1.

- Add some presets.
- Add another demo tune to go with it.
- Split oscillator AM/FM gui to separate tabs.

### July 1, 2024 - V1.8.0.

- Added logging.
- Add some more presets.
- Add another demo tune to go with it.
- Renamed unipolar acid preset.
- Switched to static linking of MSVCRT on windows.
- Added backwards FM + renamed bipolar -> through-zero + renamed unipolar -> forwards.
- Bugfix: FX version tried to read global unison params causing the plugin to crash on windows 11.
- Changed location of user settings (gui size and selected theme). Also these are no longer shared between clap/vst3.

### June 11, 2024 - V1.7.9 (No binaries).

- Add some presets.
- Add a demo tune to go with it.

### June 8, 2024 - V1.7.8 (With binaries).
### June 6, 2024 - V1.7.8 (No binaries).

- Bugfix + breaking change: 
Oscillator gain modulation was applied on osc-unison-sum level,
but AM and FM are applied per osc-unison-voice. This basically
rendered per-oscillator gain useless for AM/FM amount control.
The new setup much more closely resembles the FM "operator"
approach, but is unfortunately a breaking change.

### June 3, 2024 - V1.7.7.

- Flipped automation smoothing / other smoothing gui.
- Changed linux distribution to .zip instead of .tar.gz.
- Don't crash on missing resources, just make the gui look ugly.
- Add readme.txt that explains to copy all resources, not just the binary.

### May 22, 2024 - V1.7.6 (With binaries).
### May 16, 2024 - V1.7.6 (No binaries).

- Add some presets.

### May 9, 2024 - V1.7.5.

- Add CLAP modulation support. Only global, not per-voice yet!
- Add automation smoothing parameter in addition to MIDI and BPM smoothing. Reacts to CLAP modulators too, this is mainly useful for host-provided square lfo's etc.
- Breaking change: changed the way parameter smoothing works. This will not break the plugin proper but may have small audible differences w.r.t. previous versions.

### April 29, 2024 - V1.7.4 (No binaries).

- Added waveform 13 to the list of supported hosts. No idea if some update between waveform 12 and 13 fixed it, or there was a bug on firefly's side that got fixed.
- Unfortunately still no renoise and cakewalk.

### April 28, 2024 - V1.7.3.

- Swapped envelope sustain and smoothing in the gui.
- Bugfix: Shrunk FX title size to fit on mac.
- Bugfix: Do not store plugin settings directly under linux user home folder.
- Bugfix: internal block splitting did not respect sample accurate automation.
- Bugfix: automation changes were reported back to the host, which caused automation to stop working entirely in bitwig and flstudio.

### April 22, 2024 - V1.7.2.

- Updated dependencies to latest (juce, vst3, clap).
- Bugfix: the windows .zip archives are now actually .zip archives, not zipped .tar files.
- Breaking change: reduced max oversampling to 4x to cut down on memory usage.
- Breaking change: reduced max voice count from 128 to 64 (more memory cutdown). Global unison voices also come out of the 64 available!
- Split up internal processing into multiple fixed size buffers to not have memory usage dependent on the host buffer size.
-- This costs a bit of cpu, hopefully not too bad.

### April 13, 2024 - V1.7.1.

- Changed built-in themes (and dropped some).

### April 10, 2024 - V1.7.0.

- Rearranged the gui to have elements more aligned.
- Changed default distortion mode to plain tanh clipper.
- Renamed distortion skew in/out to x/y to align with lfo.
- Add additional global aux parameter just to fit the gui.
- Bugfix: disable envelope trigger for polyphonic mode.
- Bugfix: disable global unison parameters when voice count is 1.
- Bugfix + breaking change: when global unison is enabled voices are attenuated by sqrt(voice-count).

### April 2, 2024 - V1.6.1.

- Add Intel Mac support.

### March 31, 2024 - V1.6.0.

- Add Mac support. Apple silicon only, no Intel Macs yet!

### March 28, 2024 - V1.5.1 (No binaries).

- Add new preset (big pad 3 with global unison).

### March 27, 2024 - V1.5.0.

- Add global unison support. 
  Heavy on the cpu, benefits from CLAP threadpool.

### March 24, 2024 - V1.4.1.

- Add some color schemes.
- Add version info to gui.
- Add system DPI awareness.
- Increase undo history size.
- Organize presets in folders.
- Copy/paste patch from system clipboard.

### March 18, 2024 - V1.4.0.

- Add some soft clippers.

### March 15, 2024 - V1.3.1.

- Add some themes (just basic color schemes).

### March 14, 2024 - V1.3.0.

- Add theming support.
- Add 4 themes: current, current flat, infernal, infernal flat (re-introduced everyone's favourite creepy images!)
- Added a new preset.

### March 10, 2024 - V1.2.4 (No binaries).

- Swap out native message boxes for juce alert windows so they react to content scaling.

### March 10, 2024 - V1.2.3 (No binaries).

- Bugfix: undo menu did not react to content scaling.

### March 9, 2024 - V1.2.2 (No binaries).

- Change max ui scale factor from 200% to 800%.

### March 8, 2024 - V1.2.1.

- Add big pad 2 preset.
- Scale back default ui width to 1280.
- Bugfix: osc graph scales to 100% * gain.
- Bugfix: default global SVF keyboard tracking to 0.
- UI scaling from 50% to 200% (was 100 to unlimited).

### March 6, 2024 - V1.2.0.

- Lots of small ui fixes / cleanup.
- Rename saw waveshaper to off.
- Add 2 extra master cv aux parameters.
- Fixed a bug in the triangle waveshaper.
- Drop square waveshaper (bug, it shouldn't have been there).
- Comb filter gets additional mode select (feedback/feedforward/both).
- Split out combined comboboxes to separate controls:
  - NOTE! This is a breaking change for automation on those parameters.
  - LFO mode + sync.
  - Delay mode + sync.
  - LFO shape + skew controls.
  - FX Type + distortion mode.
  - Envelope type + slope mode.
  - Distortion shaper + skew controls.
- Rearranged lfo/distortion shape options
  - NOTE! This is a breaking change for automation on this parameter.

Even though there are some breaking changes I decided *against* changing the plugin id,
because breaking changes relate to automation of discrete-valued parameters only.
Regular loading of projects/preset files applies the appropriate conversion from previous formats.

### February 25, 2024 - V1.11.

- Add some more presets and fix some existing ones.

### February 25, 2024 - V1.1.

- Allow envelopes as target in CV-to-CV matrix. Envelopes can be modulated by master in section or global lfos.

### February 19, 2024 - V1.09.

- Add fx-only plugin version.

### February 14, 2024 - V1.08.

- Bugfix: reset voice manager state upon plugin (de)activate.
- Bugfix: do not assume CLAP host can do threadpool.
- Bugfix: do not assume CLAP host can do ostream with unlimited size.
- Bugfix: do not report stale value from CLAP gui to host, sync with audio first.

### February 12, 2024 - V1.07 (No binaries).

- Add another preset (matching to infernal synth's acid line).

### February 12, 2024 - V1.06.

- Add monophonic mode.
- Add another preset to go with it.

### February 10, 2024 - V1.05.

- Add some presets.
- Add an extra oscillator.
- LFO x/y become continuous parameters.
- Bugfix: Basic oscillator with no waveforms must not be processed.
- Add CV-to-CV routing matrices (modulate LFO by another LFO or envelope).

### February 8, 2024 - V1.04 (No binaries).

- Add scale and offset for CV matrices.

### February 3, 2024 - V1.03 (No binaries).

- Add manual / overview document.

### February 2, 2024 - V1.02 (No binaries).

- Add detailed parameter reference.

### January 31, 2024 - V1.01 (No binaries).

- Add per-oscillator gain control.
- Make the ui a bit wider to prevent gui clipping.
- Bugfix: do not limit whitenoise oscillator to nyquist.
- Bugfix: envelope whould incorrectly show as tempo-synced.

### January 27, 2024 - V1.0.

- Initial release.