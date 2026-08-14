# Changelog

## [0.3.7-premiere-stereo.5] - 2026-08-14

### Premiere stereo workflow

- detect conservative adjacent hard-panned `_L` / `_R` breakout pairs;
- interleave compatible mono PCM/float WAVE sources into lossless stereo;
- support shared-source dual-mono clips inside a confirmed breakout pair;
- retain ambiguous or incompatible material as separate mono tracks.

### Reliability and compatibility

- extract embedded essence through UTF-8-aware Windows file operations;
- support AAF files and media directories containing Unicode and Cyrillic;
- prevent a null source-path fallback from crashing REAPER;
- normalize Windows source paths for REAPER's project parser;
- emit complete project timecode settings;
- correct Premiere Constant Power fade mapping.

### Validation

- validated with embedded 48 kHz / 24-bit Premiere AAF in REAPER 7.78;
- validated stereo reconstruction, Unicode paths, item fades, timing, and
  safe mono fallback;
- pass automated tests and build matrix on Windows x64, macOS arm64/x64, and
  Linux x64.

Versions `.3` and `.4` were internal test builds and were not released. Their
suspected fade-related crash was later traced to embedded extraction from a
Unicode directory and fixed in `.5`.

## [0.3.7-premiere-stereo.2] - 2026-08-14

- recombine Premiere breakout pairs containing exact shared dual-mono clips
- open embedded WAVE sources with LibAAF-compatible Windows path semantics,
  including non-ASCII source names
- distinguish an unreadable WAVE path from an invalid RIFF/WAVE header in logs

## [0.3.7] - 2026-06-23

- version bump for Reapack (no changes)

## [0.3.6] - 2026-06-23

- set ruler in imported project to timecode

## [0.3.5] - 2026-06-22

- fix inverted pan on tracks in some situations

## [0.3.4] - 2026-03-28

- scroll log list to top on window open

## [0.3.3] - 2026-03-28

- add preference to display LibAAF debug messages in logger window (not recommended, but useful to see what's going on)
- fix project pan mode being set to old deprecated REAPER option
- abort import in case memory is full
- drop new messages instead of older ones when logger is full
- improve log messages in some situations

## [0.3.2] - 2026-03-26

- fix: restore log dialog resizing (v0.3.0 regression)
- fix marker/region colors in some circumstances

## [0.3.1] - 2026-03-25

### Fix:

- Use original file path if no usable path is found in AAF essence, to have opportunity to relink the media.

### Docs:

- Add info about aaf format support across daws. (on GitHub docs folder)

### Tests:

- Support automated tests against LibAAF supplied AAF files and aaftool output (can be improved, but can verify
  regressions)

### Other:

- Huge under the hood changes.

## [0.3.0] - 2026-03-24

- Add preference to fully zoom project after AAF import

## [0.2.0] - 2026-03-23

- Logger Window
- A bunch of other non user facing stuff

## [0.2.0-beta4] - 2026-03-23

- Minor refactor
- CI updates

## [0.2.0-beta3] - 2026-03-22

- Internal refactor: changed some dumb ownership models
- Fix log window stack overflow crash on Windows (oopsie)

## [0.2.0-beta2] - 2026-03-19

- Remove custom color logic from import log dialog, it's still broken.

## [0.2.0-beta1] - 2026-03-19

- Add import log
- Add preference to control when/how the log dialog opens (always, on warnings/errors, never)

## [0.1.2] - 2026-03-14

- fix possible crash in case libaaf returns nullptr in clips and timecode structs

## [0.1.1] - 2026-03-14

- Very minor fixes and cleanups

## [0.1.0] - 2026-03-14

- Initial release
