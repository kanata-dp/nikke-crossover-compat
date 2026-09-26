/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

/* Installer identification only; contains no machine or account information. */
static const char wrapper_id[] __attribute__((used)) = "NIKKE_INTL_WEBVIEW_COMPAT_V1";

/* The CAPTCHA WebView has its own host, separate from tbs_browser.exe.
 * Forward IPC arguments unchanged and append only rendering switches.
 * Never log arguments: they can contain login URLs and session tokens.
 */
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR args, int show)
{
    const wchar_t original[] = L"intl_service.original.exe";
    wchar_t path[32768], command[32768];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    DWORD result = 1;
    (void)instance; (void)previous; (void)show;
    startup.cb = sizeof(startup);
    DWORD length = GetModuleFileNameW(NULL, path, 32768);
    if (!length || length >= 32768) return 1;
    wchar_t *name = wcsrchr(path, L'\\');
    if (!name || (size_t)(name + 1 - path) + sizeof(original) / sizeof(*original) > 32768)
        return 2;
    wcscpy(name + 1, original);
    /* This launcher can omit argv[0] and pass a command line starting with '-'. */
    const wchar_t *raw = GetCommandLineW();
    const wchar_t *forward = raw[0] == L'-' ? raw : args;
    int count = _snwprintf(command, 32768,
        L"\"%ls\" %ls --disable-gpu --disable-gpu-compositing --in-process-gpu", path, forward);
    if (count < 0 || count >= 32768) return 3;
    if (!CreateProcessW(path, command, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process))
        return (int)GetLastError();
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &result);
    CloseHandle(process.hProcess);
    return (int)result;
}
