/*
 * Copyright (C) 2026 reAAF contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef REAPER_AAF_PROGRESSREPORTER_H
#define REAPER_AAF_PROGRESSREPORTER_H

// Optional progress reporting for potentially long AAF imports.
//
// The importer and the stereo recombiner call these methods only when a
// reporter has been supplied. Headless callers and unit tests pass nullptr,
// so the data pipeline stays free of any UI dependency.
struct IProgressReporter {
    virtual ~IProgressReporter() = default;

    // Enter an indeterminate phase, such as LibAAF reading the AAF object
    // model, where no sub-progress is available. text describes the phase.
    virtual void beginPhase(const char *text) = 0;

    // Switch to a determinate phase spanning [0, total] work units.
    // A total of 0 means "no measurable units" (keeps the bar indeterminate).
    virtual void setRange(unsigned total) = 0;

    // Report done completed work units. text may be null to keep the current
    // status line. For determinate phases done is clamped to the total.
    virtual void advance(unsigned done, const char *text) = 0;

    // The import finished (successfully or not); the reporter may release
    // its UI. Called at most once per import.
    virtual void finish() = 0;
};

#endif // REAPER_AAF_PROGRESSREPORTER_H
