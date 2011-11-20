////////////////////////////////////////////////////////////////////////////////
/// @brief implements the routine columnWidths with the api of the ncurseslib
///
/// @file
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/TerminalUtils.h"

#ifdef HAVE_NCURSES

#ifdef TRI_HAVE_NCURSES_CURSES_H
#include <ncurses/term.h>
#include <ncurses/curses.h>
#endif

#ifdef TRI_HAVE_CURSES_H
#include <term.h>
#include <curses.h>
#endif

#endif

#include <stdlib.h>
#include <Basics/StringUtils.h>

namespace triagens {
  namespace basics {
    namespace TerminalUtils {
#ifdef HAVE_NCURSES
      int columnsWidth () {
        int ret = 0;
        /* initializing terminfo  */
        int init_ret = setupterm(NULL, fileno(stdin), NULL);
        if (init_ret != 0) {
          return DEFAULT_COLUMNS;
        }

        union {
          char * a;
          char const * b;
        }c;
        c.b = "cols";

        ret = tigetnum(c.a);

        return ret == -1 ? DEFAULT_COLUMNS : ret;
      }
#else
      int columnsWidth () {
        static int DEFAULT_COLUMNS = 80;

        char* e = getenv("COLUMNS");

        if (e != 0) {
          int c = StringUtils::int32(e);

          if (c == 0) {
            return DEFAULT_COLUMNS;
          }

          return c;
        }

        return DEFAULT_COLUMNS;
      }
#endif
    }
  }
}
