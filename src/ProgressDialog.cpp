/*
 * Copyright (C) 2026 reAAF contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <cstdint>

#include "ProgressDialog.h"

#include "resource.h"

extern REAPER_PLUGIN_HINSTANCE g_hInst;

namespace {
constexpr unsigned kBarMaximum = 100;

// Place the dialog in the centre of REAPER's main window without activating it.
void centerOnParent(const HWND hwnd, const HWND parent) {
    if ( !parent )
        return;
    RECT parentRect{};
    RECT dialogRect{};
    if ( !GetWindowRect(parent, &parentRect) || !GetWindowRect(hwnd, &dialogRect) )
        return;
    const int width = dialogRect.right - dialogRect.left;
    const int height = dialogRect.bottom - dialogRect.top;
    const int x = parentRect.left + ((parentRect.right - parentRect.left) - width) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - height) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
} // namespace

ProgressDialog::ProgressDialog(const HWND parent) : m_parent(parent) {
    m_hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_AAF_PROGRESS), parent, dialogProc,
                               reinterpret_cast<LPARAM>(this));
    if ( !m_hwnd )
        return;
    centerOnParent(m_hwnd, m_parent);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);
}

ProgressDialog::~ProgressDialog() {
    if ( m_hwnd ) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void ProgressDialog::beginPhase(const char *text) {
    if ( m_finished )
        return;
    m_total = 0;
    m_done = 0;
    setStatus(text);
    setBar(0);
    pump();
}

void ProgressDialog::setRange(const unsigned total) {
    if ( m_finished )
        return;
    m_total = total;
    m_done = 0;
    setBar(0);
    pump();
}

void ProgressDialog::advance(const unsigned done, const char *text) {
    if ( m_finished )
        return;
    m_done = done;
    if ( text )
        setStatus(text);
    unsigned position = 0;
    if ( m_total > 0 ) {
        // SWELL defines min/max macros, so clamp without std::min.
        const uint64_t scaled = static_cast<uint64_t>(done) * kBarMaximum / m_total;
        position = scaled > kBarMaximum ? kBarMaximum : static_cast<unsigned>(scaled);
    }
    setBar(position);
    pump();
}

void ProgressDialog::finish() {
    if ( m_finished )
        return;
    m_finished = true;
    setBar(kBarMaximum);
    pump();
}

void ProgressDialog::setStatus(const char *text) const {
    if ( !m_hwnd || !text )
        return;
    SetDlgItemText(m_hwnd, IDC_PROGRESS_LABEL, text);
}

void ProgressDialog::setBar(const unsigned position) const {
    if ( !m_hwnd )
        return;
    const HWND bar = GetDlgItem(m_hwnd, IDC_PROGRESS_BAR);
    if ( !bar )
        return;
    const unsigned clamped = position > kBarMaximum ? kBarMaximum : position;
    SendMessage(bar, PBM_SETPOS, static_cast<WPARAM>(clamped), 0);
}

void ProgressDialog::pump() const {
    if ( !m_hwnd )
        return;
#ifdef _WIN32
    MSG msg{};
    while ( PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE) ) {
        if ( !IsDialogMessage(m_hwnd, &msg) ) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
#else
    // SWELL only exposes the native queue flush on macOS/Linux; REAPER owns the
    // actual run loop, so we drain queued SWELL messages instead of PeekMessage.
    SWELL_MessageQueue_Flush();
#endif
    // The PeekMessage filter targets the dialog only, so force the child
    // controls to repaint as well.
    UpdateWindow(m_hwnd);
    if ( const HWND bar = GetDlgItem(m_hwnd, IDC_PROGRESS_BAR) )
        UpdateWindow(bar);
    if ( const HWND label = GetDlgItem(m_hwnd, IDC_PROGRESS_LABEL) )
        UpdateWindow(label);
}

WDL_DLGRET CALLBACK ProgressDialog::dialogProc(const HWND hwnd, const UINT msg, const WPARAM wParam,
                                               const LPARAM lParam) {
    auto *self = reinterpret_cast<ProgressDialog *>(
        msg == WM_INITDIALOG ? lParam : GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch ( msg ) {
    case WM_INITDIALOG: {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
        if ( self ) {
            self->m_hwnd = hwnd;
            if ( const HWND bar = GetDlgItem(hwnd, IDC_PROGRESS_BAR) )
                SendMessage(bar, PBM_SETRANGE, 0, static_cast<LPARAM>(kBarMaximum << 16));
        }
        return 1;
    }

    case WM_CLOSE:
        return 1; // non-cancellable import: ignore close requests

    case WM_COMMAND:
        if ( LOWORD(wParam) == IDCANCEL )
            return 1; // swallow Esc
        return 0;

    case WM_DESTROY:
        if ( self )
            self->m_hwnd = nullptr;
        return 0;

    default:
        return 0;
    }
}
