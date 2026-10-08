# Premiere Breakout-to-Mono stereo fork

## Purpose and ownership

This edition extends `smandrap/reAAF` for AAF files exported by Adobe
Premiere Pro with embedded audio. The workflow and Premiere stereo requirement
were developed with Igor Safronov.

Repository: `https://github.com/safroigor/reAAF-premiere-stereo`

Current version: `0.3.7-premiere-stereo.6`

The validated engine remains `0.3.7-premiere-stereo.5`; `.6` only adds the import
progress window and does not change import or output behaviour.

Feature branch at initial validation: `agent/premiere-stereo-import`

Initial implementation commit: `d31d96a Support Premiere breakout stereo AAF imports`

Version `.3` corrected Premiere `CURV_PWR` / Constant Power fades to REAPER's
equal-power shape (internal index 1). Versions `.3` and `.4` were initially
suspected of causing a REAPER 7.78 crash, but a full process dump disproved that
hypothesis. Version `.5` removes the unnecessary `.4` deferred-fade
workaround while retaining the corrected fade mapping.

The actual crash was a pre-existing Unicode extraction defect. LibAAF used
narrow `fopen()` for embedded essence output. When an AAF was opened from a
folder such as `TestAAF/Тест2`, media-directory creation/extraction failed and
the importer passed a null `original_file_path` into `strrchr()`. The dump shows
the exact stack `VCRUNTIME140!strrchr -> rppSourceTypeFromPath -> AafImporter`.
Version `.5` uses UTF-8-aware directory and essence-file operations, guards
the path helper, and emits an empty source instead of dereferencing a missing
fallback path.

## Validated source material

The non-redistributable test fixture was exported by Adobe Premiere Pro 26.3. It contains
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

The customer AAF and extracted audio are confidential test data. Do not commit or
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

Version `.2` also accepts an exact same-name/same-file mono clip appearing on
both sides of an otherwise confirmed breakout pair, producing lossless dual
mono stereo. Version `.5` opens extracted sources with UTF-8-aware
filesystem paths, including names such as `Landström` and Cyrillic folders.

`src/AafImporter.cpp` runs recombination after embedded essence extraction.

`src/RppWriter.cpp` also contains two Windows/REAPER parser fixes:

- normalize emitted media paths from backslashes to forward slashes;
- emit the complete `TIMEMODE 1 5 -1 <fps> <drop-frame> 0 -1` record.

The extracted media directory is produced beside the AAF as `<AAF name>-media`.

## Import progress window

Version `.6` adds a modeless, non-cancellable progress window that is shown
before `aafi_load_file()` runs, so the user gets feedback during the previously
silent phases:

- `Reading AAF...` — indeterminate phase covering LibAAF's opaque load step,
  where no sub-progress is exposed;
- `Extracting <name>` — determinate, one step per embedded essence file;
- `Recombining <name>` — determinate, one step per confirmed `_L`/`_R` pair;
- `Writing REAPER project...` — indeterminate final phase.

`src/ProgressReporter.h` defines the `IProgressReporter` interface; the importer
and `StereoRecombiner` call it only when a reporter is supplied, so headless
tests stay UI-free. `src/ProgressDialog.{h,cpp}` implements it on top of SWELL
for Windows, macOS, and Linux. While the main thread is busy the dialog pumps
only its own messages (Windows) or drains the SWELL queue (macOS/Linux), so it
never re-enters REAPER's import callback.

Known limits:

- `aafi_load_file()` and a single `aafi_extractAudioEssenceFile()` call cannot
  report sub-progress without patching LibAAF, so the bar is per phase / per
  file rather than continuous;
- the import remains synchronous and cannot be cancelled.

## Tests

Relevant tests:

- `tests/test_stereorecombiner.cpp`: 16-bit and 24-bit interleave plus safety
  and fallback cases;
- `tests/test_premiere_breakout_integration.cpp`: optional local AAF fixture;
- existing `RppWriter`, `AafEmitter`, and `AafImporter` suites.

For the local integration fixture, configure with
`REAAF_PREMIERE_TEST_AAF=<absolute path>` and optionally
`REAAF_PREMIERE_TEST_RPP_OUTPUT=<absolute output path>`.

Current regression coverage:

- StereoRecombiner: 20 assertions in 6 cases passed;
- local Premiere integration: 20 assertions passed;
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

Use the GitHub Actions `Build` workflow on Windows. Historical `.2` artifact:

- run ID: `31801461584`
- artifact: `reaper-aaf-windows-x86_64`
- DLL size: `476672` bytes
- SHA-256: `1DD907B0E6DEA530D47DA39587EA5C0E91D577FCDC4E6CBBFBD7ADF95FBA2E8C`

The same run passed Windows x86_64, macOS arm64, macOS x86_64, and Linux x86_64
build jobs.

Withdrawn `.4` artifact (the fade workaround did not address the crash):

- commit: `8d1c51a Avoid REAPER crash when importing fade shapes`
- build run ID: `31809375276`
- tests run ID: `31809357493`
- Windows DLL size: `477696` bytes
- SHA-256: `39DC29734326D186B7B6FD7C8149A544F3426EF6C81E35B23D1CF26CBC16C76B`

Both workflows passed, but both `.2` and `.4` still crash when embedded AAF
essence is extracted beside an AAF located in a Unicode folder. Passing CI did
not reveal this because the automated paths were ASCII-only.

Validated Unicode extraction release `.5`:

- release commit: `9c0765e Promote Unicode path fix to validated version 0.3.7.5`
- build run ID: `31815007138`
- tests run ID: `31815007081`
- Windows DLL size: `478208` bytes
- SHA-256: `8F78BC08478653D35902E2950205857F318F0B38D9D79AAAADAD270ECA5B231F`

The build matrix passed on Windows x86_64, macOS arm64/x86_64, and Linux
x86_64. The automated test workflow also passed. Igor Safronov manually opened
the embedded-audio fixture from the Cyrillic `Тест2` directory in REAPER 7.78:
the project opened without a crash, `_L`/`_R` tracks recombined as stereo, and
item fades were correct. This validates the Unicode crash fix for distribution.

Progress-window release `.6`:

- release commit: `fb5ab3b Add AAF import progress window (#5)`
- release tag: `v0.3.7-premiere-stereo.6`
- Release run ID: `37792697405`
- Windows DLL size: `483328` bytes
- SHA-256: `CE627F49A485923E6C66DE7B6C1D63D379BA579D9FC66659A82ED656B8EFA534`

The full build matrix and test workflow passed. `.6` adds the import progress
window on top of the validated `.5` engine; the import and output paths are
unchanged. A manual REAPER check with a private Premiere fixture is still
recommended before the build is treated as fully field-validated.

## Premiere embedded AAF 2 GiB limit

Premiere on Windows does not support embedded-audio AAF files larger than 2 GB.
The validated failure fixture `PROBLEM.aaf` was a version 3 CFB container with
512-byte sectors and a size of `2147418624` bytes, only `65024` bytes below
2 GiB. LibAAF could open the CFB header but could not load the AAF object model.

Forensics showed that the root `properties` stream had zero length, the valid
directory entries stopped after the initial essence skeleton, the directory
chain continued into apparent media data, and it terminated with `FATSECT`
instead of `ENDOFCHAIN`. The timeline metadata was never finalized, so the file
cannot be repaired or converted to RPP. Do not commit or redistribute this
confidential fixture.

Ask the editor to re-export using one of these workflows:

1. `Separate Audio` (Adobe's recommended option for large AAFs), keeping the
   AAF and media folder together or wrapping them in one ZIP archive.
2. `Trim Audio Files` with reasonable handle frames so an embedded AAF stays
   comfortably below 2 GiB.
3. Split the sequence into multiple smaller embedded AAFs if a self-contained
   file is mandatory.

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
6. Repeat the import from a folder containing Cyrillic characters and confirm
   that `<AAF name>-media` is created beside the AAF.

## Distribution package

Binary handoff packages are intentionally not committed. The validated Windows
x64 package contains the DLL, a short Russian installation guide, version/hash
information, and the GPL license. Keep the DLL, version, hash, guide, and source
revision synchronized whenever a new package or GitHub Release is created.
