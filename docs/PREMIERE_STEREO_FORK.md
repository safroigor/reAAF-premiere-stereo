# Premiere Breakout-to-Mono stereo fork

## Purpose and ownership

This private fork extends `smandrap/reAAF` for AAF files exported by Adobe
Premiere Pro with embedded audio. The workflow and Premiere stereo requirement
were developed with Igor Safronov.

Private repository: `https://github.com/safroigor/reAAF-premiere-stereo`

Current version: `0.3.7-premiere-stereo.1`

Feature branch at initial validation: `agent/premiere-stereo-import`

Initial implementation commit: `d31d96a Support Premiere breakout stereo AAF imports`

## Validated source material

The private test fixture was exported by Adobe Premiere Pro 26.3. It contains
embedded 48 kHz, 24-bit PCM mono WAVE essence and six audio tracks:

- `VO1`
- `VO2`
- `GP_L`
- `GP_R`
- `MX1_L`
- `MX1_R`

Premiere supplied neither AAF `ChannelID` nor `MonoSourceSlotID` metadata for the
breakout pairs. Pairing therefore uses conservative structural evidence.

Expected imported result: four audio tracks (`VO1`, `VO2`, stereo `GP`, stereo
`MX1`) plus the video track. The user confirmed that the resulting AAF import
opens correctly in REAPER.

The customer AAF and extracted audio are private test data. Do not commit or
redistribute them.

## Implementation

`src/StereoRecombiner.cpp` and `.h` identify only adjacent `_L`/`_R` tracks that:

- have complementary hard-left/hard-right pan;
- have matching clip count and timeline geometry;
- have matching source offsets, fades, gain, and automation;
- contain compatible mono WAVE sources.

Compatible inputs are streamed and interleaved into a new stereo RIFF/WAVE file
without sample conversion. PCM, IEEE float, and WAVE_FORMAT_EXTENSIBLE are
supported. The original sample rate and bit depth are retained. A failed or
ambiguous match falls back to the two original mono tracks.

`src/AafImporter.cpp` runs recombination after embedded essence extraction.

`src/RppWriter.cpp` also contains two Windows/REAPER parser fixes:

- normalize emitted media paths from backslashes to forward slashes;
- emit the complete `TIMEMODE 1 5 -1 <fps> <drop-frame> 0 -1` record.

The extracted media directory is produced beside the AAF as `<AAF name>-media`.

## Tests

Relevant tests:

- `tests/test_stereorecombiner.cpp`: 16-bit and 24-bit interleave plus safety
  and fallback cases;
- `tests/test_premiere_breakout_integration.cpp`: optional private AAF fixture;
- existing `RppWriter`, `AafEmitter`, and `AafImporter` suites.

For the private integration fixture, configure with
`REAAF_PREMIERE_TEST_AAF=<absolute path>` and optionally
`REAAF_PREMIERE_TEST_RPP_OUTPUT=<absolute output path>`.

At initial validation:

- StereoRecombiner: 14 assertions in 4 cases passed;
- private Premiere integration: 20 assertions passed;
- RppWriter: 55 assertions passed;
- AafEmitter: 56 assertions passed;
- AafImporter golden tests: 65 assertions passed;
- AafImporter MR suite: 1519 assertions passed.

## Windows build requirement

Windows production builds must use Microsoft Visual C++. MinGW can compile and
load a DLL, and the importer log may look successful, but REAPER can then reject
the generated in-memory project with a parsing error. The cause is the ABI
mismatch at REAPER's C++ `ProjectStateContext` virtual interface.

Never distribute the old MinGW DLLs in `../plugin-backups`.

Use the GitHub Actions `Build` workflow on Windows. Initial validated run:

- run ID: `31793780451`
- artifact: `reaper-aaf-windows-x86_64`
- DLL size: `475136` bytes
- SHA-256: `CD284FB773D73108235CB43BA6D9B19326D486E0C070AD5036059A5143A11AF2`

The same run passed Windows x86_64, macOS arm64, macOS x86_64, and Linux x86_64
build jobs.

## Installation and manual verification

Copy the MSVC-built `reaper_reAAF.dll` to
`%APPDATA%\REAPER\UserPlugins\reaper_reAAF.dll`, restart REAPER, then use
`File > Open Project` to select an AAF. Passing the AAF on the REAPER process
command line is not a reliable smoke test because the file may be handled before
the extension finishes registering.

Verify all of the following:

1. REAPER loads the expected DLL from `UserPlugins`.
2. The import log reports the intended `_L`/`_R` recombinations.
3. The project opens without a project-parser error.
4. Stereo items play with correct channel ordering and timing.
5. Fades, offsets, automation, and unpaired mono tracks remain intact.

## Distribution package

The colleague handoff folder is `../AAF to RPP - для коллег`. It contains the
validated Windows x64 DLL, a short Russian installation guide, version/hash
information, and the GPL license. Update the DLL, version, hash, and guide
together whenever a new build is released.

