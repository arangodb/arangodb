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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/operating-system.h"

#ifdef TRI_HAVE_TERMIOS_H
#include <termios.h>
#endif

namespace arangodb::terminal_utils {

struct TerminalSize {
  int rows = 80;
  int columns = 80;
};

/// @brief returns the terminal size
TerminalSize defaultTerminalSize();

/// @brief set the visibility of stdin inputs (turn off for password entry etc.)
void setStdinVisibility(bool) noexcept;

}  // namespace arangodb::terminal_utils
