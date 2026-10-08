/*
 * Copyright (C) 2026 reAAF contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef REAPER_AAF_PROGRESSDIALOG_H
#define REAPER_AAF_PROGRESSDIALOG_H

#include "ProgressReporter.h"
#include "reaper_plugin.h"
#include "wdltypes.h"
#ifdef _WIN32
#include <commctrl.h>
#endif

// Modeless, non-cancellable progress window shown during a long AAF import.
// Owned by aaf_ImportProject(); creating it before run() means the window is
// visible during the opaque LibAAF load step, which is exactly when the user
// previously had no feedback at all.
class ProgressDialog final : public IProgressReporter {
  public:
    explicit ProgressDialog(HWND parent);

    ~ProgressDialog() override;

    ProgressDialog(const ProgressDialog &) = delete;

    ProgressDialog &operator=(const ProgressDialog &) = delete;

    ProgressDialog(ProgressDialog &&) = delete;

    ProgressDialog &operator=(ProgressDialog &&) = delete;

    void beginPhase(const char *text) override;

    void setRange(unsigned total) override;

    void advance(unsigned done, const char *text) override;

    void finish() override;

  private:
    static WDL_DLGRET CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void setStatus(const char *text) const;

    void setBar(unsigned position) const;

    // Process pending messages for this window only, so the bar repaints
    // without allowing REAPER's main window to re-enter the import callback.
    void pump() const;

    HWND m_parent = nullptr;
    HWND m_hwnd = nullptr;
    unsigned m_total = 0;
    unsigned m_done = 0;
    bool m_finished = false;
};

#endif // REAPER_AAF_PROGRESSDIALOG_H
