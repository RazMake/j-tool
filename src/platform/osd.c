#include <windows.h>
#include "osd.h"

static const char *s_osd_text;

static LRESULT CALLBACK osd_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, 1500, NULL);
        return 0;
    case WM_TIMER:
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH bg = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        int font_h = -MulDiv(28, GetDeviceCaps(hdc, LOGPIXELSY), 72);
        HFONT font = CreateFontA(font_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                 FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        HFONT old = (HFONT)SelectObject(hdc, font);

        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, s_osd_text, -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

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

int osd_show(const char *text)
{
    s_osd_text = text;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = osd_wndproc;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.lpszClassName  = "JumpOSD";
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int scr_w = GetSystemMetrics(SM_CXSCREEN);
    int scr_h = GetSystemMetrics(SM_CYSCREEN);

    /* measure text to size the window */
    HDC dc = CreateDCA("DISPLAY", NULL, NULL, NULL);
    int font_h = -MulDiv(28, GetDeviceCaps(dc, LOGPIXELSY), 72);
    HFONT font = CreateFontA(font_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                             FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE sz;
    GetTextExtentPoint32A(dc, text, (int)strlen(text), &sz);
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);

    int pad = 40;
    int win_w = sz.cx + pad * 2;
    int win_h = sz.cy + pad;
    int x = (scr_w - win_w) / 2;
    int y = (int)(scr_h * 0.8);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        "JumpOSD", NULL, WS_POPUP,
        x, y, win_w, win_h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
