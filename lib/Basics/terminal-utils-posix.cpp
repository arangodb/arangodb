////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifdef TRI_HAVE_SYS_IOCTL_H

#include <sys/ioctl.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the columns width
////////////////////////////////////////////////////////////////////////////////

TRI_TerminalSize TRI_DefaultTerminalSize() {
  unsigned short values[4];

  int ret = ioctl(0, TIOCGWINSZ, &values);
  if (ret == -1) {
    return TRI_DEFAULT_TERMINAL_SIZE;
  }
  return TRI_TerminalSize{values[0], values[1]};
}

#endif
