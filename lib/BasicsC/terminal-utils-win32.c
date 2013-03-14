////////////////////////////////////////////////////////////////////////////////
/// @brief terimnal utilities using windows
///
/// @file
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
/// @author Esteban Lombeyda
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/terminal-utils.h"

#include <windows.h>

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

int TRI_ColumnsWidth () {
  HANDLE hOut;
  CONSOLE_SCREEN_BUFFER_INFO SBInfo;

  hOut = GetStdHandle(STD_OUTPUT_HANDLE);

  if (hOut == INVALID_HANDLE_VALUE) {
    return TRI_DEFAULT_COLUMNS;
  }

  if (GetConsoleScreenBufferInfo(hOut, &SBInfo) == 0) {
    return TRI_DEFAULT_COLUMNS;
  }

  return SBInfo.dwSize.X;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
