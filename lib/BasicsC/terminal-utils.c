////////////////////////////////////////////////////////////////////////////////
/// @brief collections of terminal functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "terminal-utils.h"

#include "BasicsC/conversions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Terminal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the columns width
////////////////////////////////////////////////////////////////////////////////

#if ! defined(TRI_HAVE_SYS_IOCTL_H) && ! defined(TRI_WIN32_CONSOLE)

int TRI_ColumnsWidth (void) {
  char* e;

  e = getenv("COLUMNS");

  if (e != 0) {
    int c = (int) TRI_Int32String(e);

    if (c == 0 || TRI_errno() != TRI_ERROR_NO_ERROR) {
      return TRI_DEFAULT_COLUMNS;
    }

    return c;
  }

  return TRI_DEFAULT_COLUMNS;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief set the visibility of stdin inputs (turn off for password entry etc.)
////////////////////////////////////////////////////////////////////////////////

void TRI_SetStdinVisibility (bool visible) {
#ifdef TRI_HAVE_TERMIOS_H
  struct termios tty;

  tcgetattr(STDIN_FILENO, &tty);
  if (visible) {
    tty.c_lflag |= ECHO;
  }
  else {
    tty.c_lflag &= ~ECHO;
  }
  (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
