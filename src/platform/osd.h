#ifndef OSD_H
#define OSD_H

/*
 * Show a transparent OSD overlay with the given text.
 *
 * Creates a borderless, semi-transparent popup window:
 *   - WS_POPUP | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW
 *   - Dark background with large white text (Segoe UI, 28pt)
 *   - Positioned at center-bottom of the primary monitor
 *   - Auto-closes after ~1.5 seconds via WM_TIMER
 *
 * Runs its own message loop, then returns.
 * Returns 0 on success.
 */
int osd_show(const char *text);

#endif /* OSD_H */
