/* Test-only compatibility with Qt archives built by MinGW 6. */
#if defined(_WIN32) && __GNUC__ >= 10
#include <windows.h>
static int *crtArgc()
{
    return reinterpret_cast<int *>(GetProcAddress(GetModuleHandleW(L"msvcrt.dll"), "__argc"));
}
static char ***crtEnvironment()
{
    return reinterpret_cast<char ***>(GetProcAddress(GetModuleHandleW(L"msvcrt.dll"), "_environ"));
}
extern "C" {
int *(__cdecl *__imp___p___argc)(void) = crtArgc;
char ***(__cdecl *__imp___p__environ)(void) = crtEnvironment;
}
#endif
