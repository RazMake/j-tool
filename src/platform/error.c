/*
 * error.c — Error reporting implementation for Jump.
 *
 * Two output modes determined at init time:
 *   - Console mode: red text on stderr via Win32 console attributes.
 *   - GUI mode: a layered popup window (similar to the OSD in osd.c)
 *     with red text on a dark background, auto-dismissed after 5s.
 */

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "error.h"
#include "log.h"

/* Nonzero when the process has an attached console window. */
static int s_is_console = 0;

/* Formatted error message passed to the popup window proc. */
static const char *s_error_text;

/* Buffer size for vsnprintf-formatted error messages. */
#define ERROR_BUF_SIZE 2048

/* ------------------------------------------------------------------ */
/*  GUI popup (used in GUI mode)                                      */
/* ------------------------------------------------------------------ */

/*
 * create_error_font — Create the font used in the error popup.
 *
 * Uses Segoe UI at 14pt, scaled to the DC's logical DPI.
 */
static HFONT create_error_font(HDC hdc) {
    int h = -MulDiv(14, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    return CreateFontA(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

/*
 * error_wndproc — Window procedure for the error popup.
 *
 * Handles:
 *   WM_CREATE  — starts a 5-second auto-dismiss timer.
 *   WM_TIMER   — destroys the window when the timer fires.
 *   WM_KEYDOWN — allows Escape to dismiss immediately.
 *   WM_PAINT   — draws the error text in red on a dark background.
 *   WM_DESTROY — posts WM_QUIT to exit the local message loop.
 */
static LRESULT CALLBACK error_wndproc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        /* Auto-dismiss after 5 seconds. */
        SetTimer(hwnd, 1, 5000, NULL);
        return 0;

    case WM_TIMER:
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        /* Fill the entire client area with the dark background. */
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        /* Draw the error text in red, with 30px padding on all sides. */
        HFONT font = create_error_font(hdc);
        HFONT old  = (HFONT)SelectObject(hdc, font);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 80, 80));

        RECT text_rc = rc;
        text_rc.left   += 30;
        text_rc.top    += 30;
        text_rc.right  -= 30;
        text_rc.bottom -= 30;

        DrawTextA(hdc, s_error_text, -1, &text_rc,
                  DT_LEFT | DT_WORDBREAK);

        SelectObject(hdc, old);
        DeleteObject(font);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/*
 * error_show_popup — Display the error popup window and run its
 *                    message loop until the window is dismissed.
 *
 * The popup is centered horizontally and placed at 80% of screen
 * height.  It auto-sizes to fit the text (minimum 500px wide),
 * uses semi-transparency, and rounded corners.
 */
static void error_show_popup(const char *text) {
    s_error_text = text;

    /* Register a dedicated window class for error popups. */
    WNDCLASSA wc    = {0};
    wc.lpfnWndProc  = error_wndproc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = "JumpError";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    /* --- Measure the text to determine window size --- */
    HDC screen_dc = GetDC(NULL);
    HFONT font    = create_error_font(screen_dc);
    HFONT old     = (HFONT)SelectObject(screen_dc, font);

    /*
     * First pass: measure the unbounded text width so we can
     * apply the 500px minimum before measuring height.
     */
    RECT measure = {0, 0, 0, 0};
    DrawTextA(screen_dc, text, -1, &measure, DT_CALCRECT);
    int text_w = measure.right - measure.left;
    if (text_w < 500)
        text_w = 500;

    /*
     * Second pass: measure height with word-wrapping constrained
     * to the chosen text width.
     */
    RECT wrap = {0, 0, text_w, 0};
    DrawTextA(screen_dc, text, -1, &wrap, DT_CALCRECT | DT_WORDBREAK);
    int text_h = wrap.bottom - wrap.top;

    SelectObject(screen_dc, old);
    DeleteObject(font);
    ReleaseDC(NULL, screen_dc);

    /* Total window size = text size + 30px padding on each side. */
    int win_w = text_w + 60;
    int win_h = text_h + 60;

    /* Center horizontally, position at 80% of screen height. */
    int scr_w = GetSystemMetrics(SM_CXSCREEN);
    int scr_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (scr_w - win_w) / 2;
    int y = (int)(scr_h * 0.8) - win_h / 2;

    /* Create the popup window (no border, no title bar). */
    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "JumpError", NULL,
        WS_POPUP,
        x, y, win_w, win_h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd)
        return;

    /* Semi-transparent (alpha 230/255). */
    SetLayeredWindowAttributes(hwnd, 0, 230, LWA_ALPHA);

    /* Rounded corners (16px radius). */
    HRGN rgn = CreateRoundRectRgn(0, 0, win_w + 1, win_h + 1, 16, 16);
    SetWindowRgn(hwnd, rgn, TRUE);
    /* rgn is now owned by the window — do not DeleteObject. */

    /* Show and activate so it can receive keyboard input (Escape). */
    ShowWindow(hwnd, SW_SHOW);

    /* Run a local message loop until the window is destroyed. */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* ------------------------------------------------------------------ */
/*  Console red-text output (used in console mode)                    */
/* ------------------------------------------------------------------ */

/*
 * error_print_console — Print an error message to stderr in red.
 *
 * Saves the current console text attributes, switches to bright red,
 * writes the message, and restores the original attributes.
 */
static void error_print_console(const char *buf) {
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);

    /* Save original console attributes so we can restore them. */
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(handle, &info);
    WORD original_attrs = info.wAttributes;

    /* Set bright red text. */
    SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_INTENSITY);

    fprintf(stderr, "%s", buf);

    /* Restore original attributes. */
    SetConsoleTextAttribute(handle, original_attrs);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void error_init(void) {
    /*
     * If GetConsoleWindow returns non-NULL the process has a visible
     * console — use stderr output.  Otherwise fall back to the GUI
     * popup (typical for j.exe which is a Windows subsystem app).
     */
    s_is_console = (GetConsoleWindow() != NULL);
    log_write("ERR01", "error_init: mode=%s", s_is_console ? "console" : "popup");
}

void error_report(const char *fmt, ...) {
    char buf[ERROR_BUF_SIZE];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    log_write("ERR02", "error_report: %s", buf);

    if (s_is_console) {
        error_print_console(buf);
    } else {
        error_show_popup(buf);
    }
}
