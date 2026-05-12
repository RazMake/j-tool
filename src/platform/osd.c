/* On-screen display (OSD) overlay — shows a brief, semi-transparent
 * popup at the bottom of the screen with the action icon and target
 * label. Auto-dismisses after ~2 seconds. Windows GDI-based. */
#include <windows.h>
#include <string.h>
#include "osd.h"

static const char *s_osd_text;
static OsdIcon s_osd_icon;

static const wchar_t *icon_char(OsdIcon icon)
{
    switch (icon) {
    case OSD_ICON_CD:   return L"\xE8B7";   /* FolderOpen */
    case OSD_ICON_OPEN: return L"\xE71B";   /* OpenWith   */
    case OSD_ICON_EXEC: return L"\xE768";   /* Play       */
    default:            return L"";
    }
}

static HFONT create_text_font(HDC hdc)
{
    int h = -MulDiv(28, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    return CreateFontA(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

static HFONT create_icon_font(HDC hdc)
{
    int h = -MulDiv(28, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    return CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
}

static LRESULT CALLBACK osd_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, 2000, NULL);
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

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 220, 40));

        int pad = 40;
        int gap = 12;

        /* Draw icon */
        const wchar_t *icon = icon_char(s_osd_icon);
        HFONT icon_font = create_icon_font(hdc);
        HFONT old = (HFONT)SelectObject(hdc, icon_font);

        SIZE icon_sz;
        GetTextExtentPoint32W(hdc, icon, (int)wcslen(icon), &icon_sz);

        RECT icon_rc = rc;
        icon_rc.left = pad;
        icon_rc.right = pad + icon_sz.cx;
        DrawTextW(hdc, icon, -1, &icon_rc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, old);
        DeleteObject(icon_font);

        /* Draw text */
        HFONT text_font = create_text_font(hdc);
        old = (HFONT)SelectObject(hdc, text_font);

        RECT text_rc = rc;
        text_rc.left = pad + icon_sz.cx + gap;
        text_rc.right = rc.right - pad;
        DrawTextA(hdc, s_osd_text, -1, &text_rc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, old);
        DeleteObject(text_font);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int osd_show(const char *text, OsdIcon icon)
{
    s_osd_text = text;
    s_osd_icon = icon;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = osd_wndproc;
    wc.hInstance      = GetModuleHandle(NULL);
    wc.lpszClassName  = "JumpOSD";
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int scr_w = GetSystemMetrics(SM_CXSCREEN);
    int scr_h = GetSystemMetrics(SM_CYSCREEN);

    /* measure icon */
    HDC dc = CreateDCA("DISPLAY", NULL, NULL, NULL);
    HFONT icon_font = create_icon_font(dc);
    HFONT old = (HFONT)SelectObject(dc, icon_font);
    const wchar_t *ic = icon_char(icon);
    SIZE icon_sz;
    GetTextExtentPoint32W(dc, ic, (int)wcslen(ic), &icon_sz);
    SelectObject(dc, old);
    DeleteObject(icon_font);

    /* measure text */
    HFONT text_font = create_text_font(dc);
    old = (HFONT)SelectObject(dc, text_font);
    SIZE text_sz;
    GetTextExtentPoint32A(dc, text, (int)strlen(text), &text_sz);
    SelectObject(dc, old);
    DeleteObject(text_font);
    DeleteDC(dc);

    int pad = 40;
    int gap = 12;
    int win_w = pad + icon_sz.cx + gap + text_sz.cx + pad;
    int win_h = text_sz.cy + pad;
    int x = (scr_w - win_w) / 2;
    int y = (int)(scr_h * 0.8);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        "JumpOSD", NULL, WS_POPUP,
        x, y, win_w, win_h,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    /* Rounded corners */
    HRGN rgn = CreateRoundRectRgn(0, 0, win_w + 1, win_h + 1, 20, 20);
    SetWindowRgn(hwnd, rgn, TRUE);

    SetLayeredWindowAttributes(hwnd, 0, 230, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
