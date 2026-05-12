/* Windows GUI (subsystem:windows) entry point for the Jump tool.
 * Uses WinMain so the process has no console window, enabling
 * silent OSD notifications. Delegates to jump_main() like main.c. */
#include <windows.h>
#include <stdlib.h>
#include "jump.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return jump_main(__argc, __argv);
}
