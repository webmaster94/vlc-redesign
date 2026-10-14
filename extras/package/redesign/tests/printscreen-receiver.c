/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Isolated CI fixture: an external app registers the same OS hotkey as a
 * screenshot utility. It never captures images or accesses the clipboard. */
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdio.h>
static unsigned received;
static LRESULT CALLBACK messages(HWND window, UINT message, WPARAM key, LPARAM value)
{
    if (message == WM_HOTKEY)
    {
        ++received;
        printf("PrintScreen received: %u\n", received); fflush(stdout);
        if (received == 2) PostQuitMessage(0);
        return 0;
    }
    if (message == WM_TIMER) { PostQuitMessage(1); return 0; }
    return DefWindowProcW(window, message, key, value);
}
int main(void)
{
    const WNDCLASSW cls = {.lpfnWndProc = messages, .hInstance = GetModuleHandleW(NULL), .lpszClassName = L"ReframePrintProbe"};
    if (!RegisterClassW(&cls)) return 2;
    HWND window = CreateWindowW(cls.lpszClassName, L"PrintScreen test receiver", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, cls.hInstance, NULL);
    if (!window || !RegisterHotKey(window, 1, MOD_NOREPEAT, VK_SNAPSHOT)) return 3;
    SetTimer(window, 1, 120000, NULL);
    puts("READY: external PrintScreen receiver"); fflush(stdout);
    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    UnregisterHotKey(window, 1); DestroyWindow(window);
    if (received != 2) return 1;
    puts("PASS: PrintScreen forwarded from fullscreen library and player");
    return 0;
}
