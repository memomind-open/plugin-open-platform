/*
 * Windows launcher for the GM Plugin SDK build driver.
 *
 * Build from the SDK root with:
 * x86_64-w64-mingw32-gcc -O2 -s -municode -o gm-build.exe \
 *   tools/windows/gm_build_launcher.c
 */

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static void append_text(wchar_t **cursor, const wchar_t *text) {
  while (*text != L'\0') {
    *(*cursor)++ = *text++;
  }
}

/* Quote one argv element using Windows CommandLineToArgvW-compatible rules. */
static void append_argument(wchar_t **cursor, const wchar_t *argument) {
  const wchar_t *character;
  unsigned int backslashes;
  unsigned int emitted_backslashes;

  *(*cursor)++ = L'"';
  for (character = argument; *character != L'\0'; ++character) {
    if (*character != L'\\') {
      if (*character == L'"') {
        *(*cursor)++ = L'\\';
      }
      *(*cursor)++ = *character;
      continue;
    }

    backslashes = 0;
    while (character[backslashes] == L'\\') {
      ++backslashes;
    }
    if (character[backslashes] == L'"' || character[backslashes] == L'\0') {
      emitted_backslashes = backslashes * 2;
    } else {
      emitted_backslashes = backslashes;
    }
    while (emitted_backslashes-- > 0) {
      *(*cursor)++ = L'\\';
    }
    character += backslashes;
    --character;
  }
  *(*cursor)++ = L'"';
}

static wchar_t *make_command_line(const wchar_t *executable,
                                  const wchar_t *arguments) {
  size_t capacity = (wcslen(executable) + wcslen(arguments) + 4) * 2 + 1;
  wchar_t *command_line = calloc(capacity, sizeof(*command_line));
  wchar_t *cursor;

  if (command_line == NULL) {
    return NULL;
  }
  cursor = command_line;
  append_argument(&cursor, executable);
  *cursor++ = L' ';
  append_text(&cursor, arguments);
  *cursor = L'\0';
  return command_line;
}

static DWORD run_process(const wchar_t *executable, const wchar_t *arguments) {
  STARTUPINFOW startup_info = {0};
  PROCESS_INFORMATION process_information = {0};
  wchar_t *command_line = make_command_line(executable, arguments);
  DWORD exit_code = 1;

  if (command_line == NULL) {
    fwprintf(stderr, L"gm-build: unable to allocate the Python command line.\n");
    return exit_code;
  }

  startup_info.cb = sizeof(startup_info);
  if (!CreateProcessW(executable, command_line, NULL, NULL, TRUE, 0, NULL, NULL,
                      &startup_info, &process_information)) {
    fwprintf(stderr, L"gm-build: could not start %ls (Windows error %lu).\n",
             executable, GetLastError());
    free(command_line);
    return exit_code;
  }

  WaitForSingleObject(process_information.hProcess, INFINITE);
  GetExitCodeProcess(process_information.hProcess, &exit_code);
  CloseHandle(process_information.hThread);
  CloseHandle(process_information.hProcess);
  free(command_line);
  return exit_code;
}

static BOOL find_on_path(const wchar_t *file_name, wchar_t *path, DWORD size) {
  DWORD result = SearchPathW(NULL, file_name, NULL, size, path, NULL);
  return result > 0 && result < size;
}

static BOOL find_user_python(wchar_t *path, DWORD size) {
  wchar_t local_app_data[MAX_PATH];
  wchar_t pattern[MAX_PATH];
  WIN32_FIND_DATAW entry;
  HANDLE handle;
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data,
                                          ARRAYSIZE(local_app_data));

  if (length == 0 || length >= ARRAYSIZE(local_app_data)) {
    return FALSE;
  }
  if (swprintf(pattern, ARRAYSIZE(pattern), L"%ls\\Programs\\Python\\Python3*",
               local_app_data) < 0) {
    return FALSE;
  }

  handle = FindFirstFileW(pattern, &entry);
  if (handle == INVALID_HANDLE_VALUE) {
    return FALSE;
  }
  do {
    if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      continue;
    }
    if (swprintf(path, size, L"%ls\\Programs\\Python\\%ls\\python.exe",
                 local_app_data, entry.cFileName) >= 0 &&
        GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
      FindClose(handle);
      return TRUE;
    }
  } while (FindNextFileW(handle, &entry));

  FindClose(handle);
  return FALSE;
}

static wchar_t *build_python_arguments(const wchar_t *driver, int argc,
                                       wchar_t **argv, BOOL use_python_launcher) {
  size_t capacity = wcslen(driver) * 2 + 32;
  wchar_t *arguments;
  wchar_t *cursor;
  int index;

  for (index = 1; index < argc; ++index) {
    capacity += wcslen(argv[index]) * 2 + 3;
  }
  arguments = calloc(capacity, sizeof(*arguments));
  if (arguments == NULL) {
    return NULL;
  }

  cursor = arguments;
  if (use_python_launcher) {
    append_text(&cursor, L"-3 ");
  }
  append_argument(&cursor, driver);
  for (index = 1; index < argc; ++index) {
    *cursor++ = L' ';
    append_argument(&cursor, argv[index]);
  }
  *cursor = L'\0';
  return arguments;
}

int wmain(void) {
  wchar_t launcher_path[MAX_PATH];
  wchar_t driver_path[MAX_PATH];
  wchar_t python_path[MAX_PATH];
  wchar_t winget_path[MAX_PATH];
  wchar_t *arguments;
  wchar_t **argv;
  int argc;
  wchar_t *last_slash;
  BOOL use_python_launcher;
  DWORD result;

  if (GetModuleFileNameW(NULL, launcher_path, ARRAYSIZE(launcher_path)) == 0) {
    fwprintf(stderr, L"gm-build: could not locate the SDK directory.\n");
    return 1;
  }
  last_slash = wcsrchr(launcher_path, L'\\');
  if (last_slash == NULL ||
      swprintf(driver_path, ARRAYSIZE(driver_path), L"%.*ls\\tools\\gm_build.py",
               (int)(last_slash - launcher_path), launcher_path) < 0) {
    fwprintf(stderr, L"gm-build: SDK path is too long.\n");
    return 1;
  }

  argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == NULL) {
    fwprintf(stderr, L"gm-build: could not read command arguments.\n");
    return 1;
  }

  use_python_launcher = find_on_path(L"py.exe", python_path, ARRAYSIZE(python_path));
  if (!use_python_launcher &&
      !find_on_path(L"python.exe", python_path, ARRAYSIZE(python_path))) {
    if (!find_on_path(L"winget.exe", winget_path, ARRAYSIZE(winget_path))) {
      fwprintf(stderr, L"gm-build: Python 3.8 or newer is required. Install Python, then run .\\gm-build again.\n");
      LocalFree(argv);
      return 1;
    }
    fputws(L"Python was not found. Installing Python 3.12...\n", stdout);
    result = run_process(winget_path,
                         L"install --exact --id Python.Python.3.12 "
                         L"--accept-package-agreements --accept-source-agreements");
    if (result != 0) {
      LocalFree(argv);
      return (int)result;
    }
    use_python_launcher = find_on_path(L"py.exe", python_path, ARRAYSIZE(python_path));
    if (!use_python_launcher &&
        !find_on_path(L"python.exe", python_path, ARRAYSIZE(python_path)) &&
        !find_user_python(python_path, ARRAYSIZE(python_path))) {
      fwprintf(stderr, L"gm-build: Python was installed but is not available yet. Close and reopen PowerShell, then run .\\gm-build again.\n");
      LocalFree(argv);
      return 1;
    }
  }

  arguments = build_python_arguments(driver_path, argc, argv, use_python_launcher);
  LocalFree(argv);
  if (arguments == NULL) {
    fwprintf(stderr, L"gm-build: unable to allocate Python arguments.\n");
    return 1;
  }
  result = run_process(python_path, arguments);
  free(arguments);
  return (int)result;
}
