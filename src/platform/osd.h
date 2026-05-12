#ifndef OSD_H
#define OSD_H

typedef enum {
    OSD_ICON_CD,
    OSD_ICON_OPEN,
    OSD_ICON_EXEC
} OsdIcon;

/*
 * Show a transparent OSD overlay with the given text and action icon.
 *
 * Creates a borderless, semi-transparent popup window:
 *   - WS_POPUP | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW
 *   - Dark background with rounded corners, yellow text (Segoe UI, 28pt)
 *   - Action icon (CD/OPEN/EXEC) drawn via Segoe MDL2 Assets
 *   - Positioned at center-bottom of the primary monitor
 *   - Auto-closes after ~1.5 seconds via WM_TIMER
 *
 * Runs its own message loop, then returns.
 * Returns 0 on success.
 */
int osd_show(const char *text, OsdIcon icon);

#endif /* OSD_H */
