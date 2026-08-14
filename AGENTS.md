# reAAF Premiere stereo fork notes

This is the Premiere stereo edition maintained with Igor Safronov. Read
`docs/PREMIERE_STEREO_FORK.md` before investigating, building, packaging, or
changing the importer.

Key rules for future work:

- Never ship a Windows DLL compiled with MinGW. The plugin calls REAPER C++
  virtual interfaces and must use the Microsoft Visual C++ ABI.
- Preserve ambiguous Premiere `_L`/`_R` material as mono. Stereo recombination
  must remain conservative and lossless.
- Never commit customer AAF files, extracted media, DLLs, build directories, or
  crash dumps.
- Embedded essence extraction on Windows must remain UTF-8-aware. Version
  `0.3.7-premiere-stereo.5` fixed a REAPER crash caused by an AAF located in a
  Unicode/Cyrillic directory and a null fallback source path.
- Premiere on Windows cannot reliably produce embedded-audio AAF near or above
  2 GiB. Recommend `Separate Audio`, a smaller trimmed embedded export, or split
  sequences. See `docs/PREMIERE_STEREO_FORK.md` for the validated corrupt case.
- The validated source branch is `agent/premiere-stereo-import`.
- The repository is
  `https://github.com/safroigor/reAAF-premiere-stereo`.
- The validated Windows artifact and handoff package are recorded in
  `docs/PREMIERE_STEREO_FORK.md`.
