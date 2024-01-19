////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Esteban Lombeyda
////////////////////////////////////////////////////////////////////////////////

#include "Basics/terminal-utils.h"

#include <windows.h>

namespace arangodb::terminal_utils {

/// @brief returns the terminal size
TerminalSize defaultTerminalSize() {
  TerminalSize result;

  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

  if (hOut == INVALID_HANDLE_VALUE) {
    return result;
  }

  CONSOLE_SCREEN_BUFFER_INFO SBInfo;
  if (GetConsoleScreenBufferInfo(hOut, &SBInfo) == 0) {
    return result;
  }

  result.rows = SBInfo.dwSize.Y;
  result.columns = SBInfo.dwSize.X;

  return result;
}

}  // namespace arangodb::terminal_utils
