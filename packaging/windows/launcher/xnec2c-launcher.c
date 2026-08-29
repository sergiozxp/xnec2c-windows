#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>

#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

#define PATH_CAPACITY 32768

static BOOL
join_path(wchar_t *destination, size_t capacity, const wchar_t *root,
  const wchar_t *suffix)
{
  size_t root_length = wcslen(root);
  size_t suffix_length = wcslen(suffix);

  if (root_length + suffix_length + 1 > capacity)
    return FALSE;

  memcpy(destination, root, root_length * sizeof(*destination));
  memcpy(destination + root_length, suffix,
    (suffix_length + 1) * sizeof(*destination));
  return TRUE;
}

static BOOL
path_exists(const wchar_t *path)
{
  return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void
show_error(const wchar_t *summary, DWORD error)
{
  wchar_t system_message[1024] = L"";
  wchar_t message[2048];
  const wchar_t *hint = L"Keep the complete portable Xnec2c folder together, "
    L"including its bin, lib and share directories.";

  if (error != ERROR_SUCCESS)
  {
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, error, 0, system_message,
      (DWORD)(sizeof(system_message) / sizeof(system_message[0])), NULL);
  }

  if (error == ERROR_SUCCESS)
  {
    _snwprintf_s(message, sizeof(message) / sizeof(message[0]), _TRUNCATE,
      L"%ls\n\n%ls", summary, hint);
  }
  else
  {
    _snwprintf_s(message, sizeof(message) / sizeof(message[0]), _TRUNCATE,
      L"%ls\n\n%ls\n\nWindows error %lu: %ls",
      summary, hint, (unsigned long)error, system_message);
  }

  MessageBoxW(NULL, message, L"Xnec2c could not start",
    MB_OK | MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND);
}

static BOOL
get_launcher_directory(wchar_t *directory, size_t capacity)
{
  DWORD length = GetModuleFileNameW(NULL, directory, (DWORD)capacity);
  wchar_t *separator;

  if (length == 0 || length >= capacity)
    return FALSE;

  separator = wcsrchr(directory, L'\\');
  if (separator == NULL)
  {
    SetLastError(ERROR_BAD_PATHNAME);
    return FALSE;
  }

  *separator = L'\0';
  return TRUE;
}

static BOOL
prepend_bin_to_path(const wchar_t *root)
{
  DWORD old_capacity;
  DWORD copied;
  size_t root_length = wcslen(root);
  size_t bin_length = root_length + wcslen(L"\\bin");
  size_t total_capacity;
  wchar_t *value;

  SetLastError(ERROR_SUCCESS);
  old_capacity = GetEnvironmentVariableW(L"PATH", NULL, 0);
  if (old_capacity == 0 && GetLastError() != ERROR_SUCCESS &&
      GetLastError() != ERROR_ENVVAR_NOT_FOUND)
    return FALSE;

  total_capacity = bin_length + 1;
  if (old_capacity > 0)
    total_capacity += 1 + old_capacity;

  value = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
    total_capacity * sizeof(*value));
  if (value == NULL)
  {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }

  memcpy(value, root, root_length * sizeof(*value));
  memcpy(value + root_length, L"\\bin", 5 * sizeof(*value));

  if (old_capacity > 0)
  {
    value[bin_length] = L';';
    copied = GetEnvironmentVariableW(L"PATH", value + bin_length + 1,
      old_capacity);
    if (copied == 0 || copied >= old_capacity)
    {
      DWORD error = GetLastError();
      HeapFree(GetProcessHeap(), 0, value);
      SetLastError(error == ERROR_SUCCESS ? ERROR_INSUFFICIENT_BUFFER : error);
      return FALSE;
    }
  }

  if (!SetEnvironmentVariableW(L"PATH", value))
  {
    DWORD error = GetLastError();
    HeapFree(GetProcessHeap(), 0, value);
    SetLastError(error);
    return FALSE;
  }

  HeapFree(GetProcessHeap(), 0, value);
  return TRUE;
}

static BOOL
set_home_if_missing(void)
{
  wchar_t profile[PATH_CAPACITY];
  DWORD length;

  SetLastError(ERROR_SUCCESS);
  length = GetEnvironmentVariableW(L"HOME", NULL, 0);
  if (length > 0)
    return TRUE;
  if (GetLastError() != ERROR_SUCCESS &&
      GetLastError() != ERROR_ENVVAR_NOT_FOUND)
    return FALSE;

  length = GetEnvironmentVariableW(L"USERPROFILE", profile,
    (DWORD)(sizeof(profile) / sizeof(profile[0])));
  if (length == 0 || length >= sizeof(profile) / sizeof(profile[0]))
    return FALSE;

  return SetEnvironmentVariableW(L"HOME", profile);
}

static BOOL
set_relative_environment(const wchar_t *name, const wchar_t *root,
  const wchar_t *suffix, BOOL only_if_present)
{
  wchar_t value[PATH_CAPACITY];

  if (!join_path(value, sizeof(value) / sizeof(value[0]), root, suffix))
  {
    SetLastError(ERROR_BUFFER_OVERFLOW);
    return FALSE;
  }

  if (only_if_present && !path_exists(value))
    return TRUE;

  return SetEnvironmentVariableW(name, value);
}

static BOOL
configure_child_environment(const wchar_t *root)
{
  if (!prepend_bin_to_path(root) || !set_home_if_missing())
    return FALSE;

  if (!set_relative_environment(L"GDK_PIXBUF_MODULE_FILE", root,
      L"\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache", TRUE))
    return FALSE;
  if (!set_relative_environment(L"GTK_IM_MODULE_FILE", root,
      L"\\lib\\gtk-3.0\\3.0.0\\immodules.cache", TRUE))
    return FALSE;
  if (!set_relative_environment(L"GSETTINGS_SCHEMA_DIR", root,
      L"\\share\\glib-2.0\\schemas", FALSE))
    return FALSE;
  if (!set_relative_environment(L"XNEC2C_LOCALEDIR", root,
      L"\\share\\locale", FALSE))
    return FALSE;

  return TRUE;
}

static wchar_t *
build_command_line(const wchar_t *executable, const wchar_t *arguments)
{
  size_t executable_length = wcslen(executable);
  size_t arguments_length = arguments == NULL ? 0 : wcslen(arguments);
  size_t capacity = executable_length + arguments_length + 5;
  wchar_t *command_line = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
    capacity * sizeof(*command_line));
  wchar_t *cursor;

  if (command_line == NULL)
  {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
  }

  cursor = command_line;
  *cursor++ = L'"';
  memcpy(cursor, executable, executable_length * sizeof(*cursor));
  cursor += executable_length;
  *cursor++ = L'"';

  if (arguments_length > 0)
  {
    *cursor++ = L' ';
    memcpy(cursor, arguments, arguments_length * sizeof(*cursor));
    cursor += arguments_length;
  }
  *cursor = L'\0';

  return command_line;
}

int WINAPI
wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
  PWSTR command_tail, int show_command)
{
  wchar_t root[PATH_CAPACITY];
  wchar_t executable[PATH_CAPACITY];
  wchar_t critical_dll[PATH_CAPACITY];
  wchar_t *command_line;
  STARTUPINFOW startup = { 0 };
  PROCESS_INFORMATION process = { 0 };
  DWORD exit_code = ERROR_GEN_FAILURE;
  DWORD error;

  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(previous_instance);
  UNREFERENCED_PARAMETER(show_command);

  startup.cb = sizeof(startup);

  if (!get_launcher_directory(root,
      sizeof(root) / sizeof(root[0])))
  {
    show_error(L"The launcher could not determine its own folder.",
      GetLastError());
    return 1;
  }

  if (!join_path(executable, sizeof(executable) / sizeof(executable[0]),
      root, L"\\bin\\xnec2c.exe") || !path_exists(executable))
  {
    show_error(L"bin\\xnec2c.exe is missing from the portable package.",
      ERROR_FILE_NOT_FOUND);
    return 1;
  }

  if (!join_path(critical_dll,
      sizeof(critical_dll) / sizeof(critical_dll[0]), root,
      L"\\bin\\libgtk-3-0.dll") || !path_exists(critical_dll))
  {
    show_error(L"The critical GTK runtime bin\\libgtk-3-0.dll is missing.",
      ERROR_MOD_NOT_FOUND);
    return 1;
  }

  if (!join_path(critical_dll,
      sizeof(critical_dll) / sizeof(critical_dll[0]), root,
      L"\\bin\\libglib-2.0-0.dll") || !path_exists(critical_dll))
  {
    show_error(L"The critical GLib runtime bin\\libglib-2.0-0.dll is missing.",
      ERROR_MOD_NOT_FOUND);
    return 1;
  }

  if (!configure_child_environment(root))
  {
    show_error(L"The launcher could not prepare the private GTK environment.",
      GetLastError());
    return 1;
  }

  command_line = build_command_line(executable, command_tail);
  if (command_line == NULL)
  {
    show_error(L"The launcher could not prepare the command line.",
      GetLastError());
    return 1;
  }

  if (!CreateProcessW(executable, command_line, NULL, NULL, FALSE,
      CREATE_NO_WINDOW, NULL, root, &startup, &process))
  {
    error = GetLastError();
    HeapFree(GetProcessHeap(), 0, command_line);
    show_error(L"Windows could not load bin\\xnec2c.exe or one of its DLLs.",
      error);
    return 1;
  }

  HeapFree(GetProcessHeap(), 0, command_line);
  CloseHandle(process.hThread);

  if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0 ||
      !GetExitCodeProcess(process.hProcess, &exit_code))
  {
    error = GetLastError();
    CloseHandle(process.hProcess);
    show_error(L"Xnec2c started, but the launcher could not read its result.",
      error);
    return 1;
  }

  CloseHandle(process.hProcess);
  return (int)exit_code;
}
