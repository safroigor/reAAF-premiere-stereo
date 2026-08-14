# reAAF Premiere Stereo

[![Build](https://github.com/safroigor/reAAF-premiere-stereo/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/safroigor/reAAF-premiere-stereo/actions/workflows/build.yml)
[![Tests](https://github.com/safroigor/reAAF-premiere-stereo/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/safroigor/reAAF-premiere-stereo/actions/workflows/tests.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

An unofficial REAPER extension for opening AAF sessions, with dedicated support
for embedded-audio AAF exports from Adobe Premiere Pro.

[Русское описание и инструкция](README.ru.md)

Current validated version: **0.3.7-premiere-stereo.5**

## What this edition adds

- opens AAF files directly through REAPER's **File → Open Project** dialog;
- extracts audio embedded in the AAF beside the source file;
- conservatively detects adjacent Premiere Breakout-to-Mono `_L` / `_R` tracks;
- interleaves compatible mono WAVE essence into a lossless stereo WAVE file;
- preserves clip timing, source offsets, gain, fades, automation, sample rate,
  and bit depth;
- supports Unicode paths and filenames on Windows, including Cyrillic folders;
- leaves ambiguous or incompatible channel pairs as mono instead of changing
  the mix silently.

The importer is read-only: it imports AAF into REAPER but does not export AAF.

## Validated workflow

The Premiere stereo workflow was developed and validated with Igor Safronov
using real Adobe Premiere Pro embedded-audio AAF projects and REAPER 7.78 on
Windows x64.

Version `0.3.7-premiere-stereo.5` was validated for:

- embedded 48 kHz / 24-bit audio;
- Premiere `_L` / `_R` breakout-track reconstruction;
- dual-mono clips inside an otherwise valid stereo pair;
- Unicode and Cyrillic AAF directories;
- item fades and project timecode;
- safe fallback when a pair cannot be proven compatible.

## Installation on Windows

1. Close REAPER.
2. Download the Windows x64 DLL or handoff package from the repository owner.
3. Copy `reaper_reAAF.dll` to `%APPDATA%\REAPER\UserPlugins`.
4. Restart REAPER.
5. Select **File → Open Project** and choose an `.aaf` file.

For embedded-audio AAF files, the importer creates `<AAF name>-media` beside
the AAF. Keep that directory with the saved REAPER project.

## Important limitations

- Only structurally verified `_L` / `_R` pairs are recombined.
- AAF can only contain what the source application exported. Plug-ins, MIDI,
  tempo maps, routing, and some automation are normally absent.
- Adobe Premiere Pro on Windows can produce an incomplete embedded AAF near the
  2 GiB limit. For large projects, prefer **Separate Audio**, trim the media
  with reasonable handles, or split the sequence.
- Marker support varies by exporter and is not guaranteed for Premiere AAF.

See [AAF format support](docs/aaf-format-support.md) for general format limits
and [Premiere Stereo maintenance notes](docs/PREMIERE_STEREO_FORK.md) for the
implementation and validated cases.

## Building

The dependencies are pinned Git submodules:

- [REAPER SDK](https://github.com/justinfrankel/reaper-sdk)
- [WDL](https://github.com/justinfrankel/WDL)
- [LibAAF](https://github.com/agfline/LibAAF)
- [Catch2](https://github.com/catchorg/Catch2)

```bash
git clone --recurse-submodules https://github.com/safroigor/reAAF-premiere-stereo.git
cd reAAF-premiere-stereo
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target reaper_aaf
```

Windows production builds must use Microsoft Visual C++. MinGW is not
supported because the extension crosses REAPER's C++ plug-in ABI.

## Tests

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target all_tests
ctest --test-dir build/tests --output-on-failure
```

Private customer AAF files are never stored in this repository. The optional
Premiere integration test is enabled only when a local fixture path is supplied.

## Credits

This edition is based on [reAAF](https://github.com/smandrap/reAAF), created by
Federico Manuppella, and uses [LibAAF](https://github.com/agfline/LibAAF) by
Adrien Gesta-Fline. Premiere stereo reconstruction, Unicode-path hardening, and
workflow validation were developed with Igor Safronov.

Adobe Premiere Pro and REAPER are trademarks of their respective owners. This
project is independent and is not affiliated with or endorsed by Adobe or
Cockos.

## License

The project is distributed under the GNU General Public License v3.0 or later.
See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).
Dependency licenses remain with their respective projects and pinned
submodules.
