////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Basics/NumberUtils.h"

#include <cstring>
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb;

namespace arangodb::terminal_utils {

/// @brief returns the terminal size
#if !defined(TRI_HAVE_SYS_IOCTL_H)
TerminalSize defaultTerminalSize() {
  auto getFromEnvironment = [](char const* name, int defaultValue) {
    char* e = getenv(name);

    if (e != 0) {
      bool valid = false;
      int value =
          static_cast<int>(NumberUtils::atoi_positive(e, e + strlen(e), valid));

      if (valid && columns != 0) {
        return value;
      }
    }
    return defaultValue;
  };

  TerminalSize result;
  result.columns = getFromEnvironment("COLUMNS", result.columns);
  result.rows = getFromEnvironment("LINES", result.rows);

  return result;
}

#endif

/// @brief set the visibility of stdin inputs (turn off for password entry etc.)
void setStdinVisibility(bool visible) noexcept {
#ifdef TRI_HAVE_TERMIOS_H
  struct termios tty;

  tcgetattr(STDIN_FILENO, &tty);
  if (visible) {
    tty.c_lflag |= ECHO;
  } else {
    tty.c_lflag &= ~ECHO;
  }
  (void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

}  // namespace arangodb::terminal_utils
