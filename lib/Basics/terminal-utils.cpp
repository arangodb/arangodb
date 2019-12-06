////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "terminal-utils.h"
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the columns width
////////////////////////////////////////////////////////////////////////////////

#if !defined(TRI_HAVE_SYS_IOCTL_H) && !defined(TRI_WIN32_CONSOLE)
TRI_TerminalSize TRI_DefaultTerminalSize() {
  char* e = getenv("COLUMNS");

  if (e != 0) {
    int columns = (int)TRI_Int32String(e);

    if (columns == 0 || TRI_errno() != TRI_ERROR_NO_ERROR) {
      return TRI_DEFAULT_TERMINAL_SIZE;
    }

    e = getenv("LINES");
    if (e != 0) {
      int rows = (int)TRI_Int32String(e);

      if (rows == 0 || TRI_errno() != TRI_ERROR_NO_ERROR) {
        return TRI_DEFAULT_TERMINAL_SIZE;
      }
      return TRI_TerminalSize{rows, columns};
    }
  }

  return TRI_DEFAULT_TERMINAL_SIZE;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief set the visibility of stdin inputs (turn off for password entry etc.)
////////////////////////////////////////////////////////////////////////////////

void TRI_SetStdinVisibility(bool visible) {
#ifdef TRI_HAVE_TERMIOS_H
  struct termios tty;

  tcgetattr(STDIN_FILENO, &tty);
  if (visible) {
    tty.c_lflag |= ECHO;
  } else {
    tty.c_lflag &= ~ECHO;
  }
  (void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#else
#ifdef _WIN32
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);

  if (visible) {
    mode |= ENABLE_ECHO_INPUT;
  } else {
    mode &= ~ENABLE_ECHO_INPUT;
  }
  SetConsoleMode(hStdin, mode);
#endif
#endif
}
