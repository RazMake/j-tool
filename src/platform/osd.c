/* On-screen display (OSD) overlay — shows a brief, semi-transparent
 * popup at the bottom of the screen with the action icon and target
 * label. Auto-dismisses after ~2 seconds. Windows GDI-based. */
#include <windows.h>
#include <string.h>
#include "osd.h"

static const char *s_osd_text;
static const char *s_osd_error;
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

static HFONT create_error_font(HDC hdc)
{
    int h = -MulDiv(14, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    return CreateFontA(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, "Segoe UI");
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

        /* Compute vertical layout */
        int main_top = 0;
        int main_bottom = rc.bottom;
        if (s_osd_error && s_osd_error[0]) {
            /* Split area: main text in upper portion, error in lower */
            HFONT err_font = create_error_font(hdc);
            HFONT old_f = (HFONT)SelectObject(hdc, err_font);
            SIZE err_sz;
            GetTextExtentPoint32A(hdc, s_osd_error,
                                 (int)strlen(s_osd_error), &err_sz);
            SelectObject(hdc, old_f);
            DeleteObject(err_font);
            main_bottom = rc.bottom - err_sz.cy - 4;
        }

        /* Draw icon */
        const wchar_t *icon = icon_char(s_osd_icon);
        HFONT icon_font = create_icon_font(hdc);
        HFONT old = (HFONT)SelectObject(hdc, icon_font);

        SIZE icon_sz;
        GetTextExtentPoint32W(hdc, icon, (int)wcslen(icon), &icon_sz);

        RECT icon_rc = rc;
        icon_rc.left = pad;
        icon_rc.right = pad + icon_sz.cx;
        icon_rc.top = main_top;
        icon_rc.bottom = main_bottom;
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
        text_rc.top = main_top;
        text_rc.bottom = main_bottom;
        DrawTextA(hdc, s_osd_text, -1, &text_rc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, old);
        DeleteObject(text_font);

        /* Draw error text if present */
        if (s_osd_error && s_osd_error[0]) {
            HFONT err_font = create_error_font(hdc);
            old = (HFONT)SelectObject(hdc, err_font);
            SetTextColor(hdc, RGB(255, 80, 80));

            RECT err_rc = rc;
            err_rc.left = pad + icon_sz.cx + gap;
            err_rc.right = rc.right - pad;
            err_rc.top = main_bottom;
            err_rc.bottom = rc.bottom;
            DrawTextA(hdc, s_osd_error, -1, &err_rc,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, old);
            DeleteObject(err_font);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int osd_show(const char *text, OsdIcon icon, const char *error)
{
    s_osd_text = text;
    s_osd_icon = icon;
    s_osd_error = error;

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

    /* measure error text */
    SIZE err_sz = {0, 0};
    if (error && error[0]) {
        HFONT err_font = create_error_font(dc);
        old = (HFONT)SelectObject(dc, err_font);
        GetTextExtentPoint32A(dc, error, (int)strlen(error), &err_sz);
        SelectObject(dc, old);
        DeleteObject(err_font);
    }
    DeleteDC(dc);

    int pad = 40;
    int gap = 12;
    int content_w = text_sz.cx;
    if (err_sz.cx > content_w) content_w = err_sz.cx;
    int win_w = pad + icon_sz.cx + gap + content_w + pad;
    int win_h = text_sz.cy + pad;
    if (err_sz.cy > 0) win_h += err_sz.cy + 4;
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
