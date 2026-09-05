#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "windows_native.h"

#ifdef XNEC2C_NATIVE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

int
windows_open_nec_resources(void)
{
  HINSTANCE result = ShellExecuteW(NULL, L"open",
      L"https://antenas.charlygolf.com/", NULL, NULL, SW_SHOWNORMAL);
  return (INT_PTR)result > 32;
}

int
windows_restart_self(void)
{
  wchar_t executable[MAX_PATH];
  STARTUPINFOW startup = {0};
  PROCESS_INFORMATION process = {0};
  DWORD length;

  length = GetModuleFileNameW(NULL, executable, MAX_PATH);
  startup.cb = sizeof(startup);

  if( length == 0 || length >= MAX_PATH ||
      !CreateProcessW(executable, NULL, NULL, NULL, FALSE, 0,
          NULL, NULL, &startup, &process) )
  {
    MessageBoxW(NULL,
        L"Xnec2c could not restart automatically. Please start it again manually.",
        L"Xnec2c restart", MB_OK | MB_ICONERROR);
    return 0;
  }

  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return 1;
}

#else

int windows_open_nec_resources(void) { return 0; }
int windows_restart_self(void) { return 0; }

#endif
