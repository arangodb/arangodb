////////////////////////////////////////////////////////////////////////////////
/// @brief shell factory
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ShellImplFactory.h"

#ifdef _WIN32
#include "LinenoiseShell.h"
#elif defined TRI_HAVE_LINENOISE
#include "LinenoiseShell.h"
#elif defined TRI_HAVE_READLINE
#include "ReadlineShell.h"
#endif
#include "DummyShell.h"

using namespace triagens;
using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                            class ShellImplFactory
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

#include <iostream>
////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shell
////////////////////////////////////////////////////////////////////////////////

ShellImplementation* ShellImplFactory::buildShell (std::string const& history, 
                                                   Completer* completer) {
  if (! isatty(STDIN_FILENO)) {
    // no keyboard input. use low-level shell without fancy color codes 
    // and with proper pipe handling
    return new DummyShell(history, completer);
  }

#ifdef _WIN32
  // under Windows the readline is not compilable
  return new LinenoiseShell(history, completer);
#elif defined TRI_HAVE_LINENOISE
  return new LinenoiseShell(history, completer);
#elif defined TRI_HAVE_READLINE
  return new ReadlineShell(history, completer);
#else
  // last resort!
  return new DummyShell(history, completer);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the shell will have a CTRL-C handler
////////////////////////////////////////////////////////////////////////////////

bool ShellImplFactory::hasCtrlCHandler () {
  if (! isatty(STDIN_FILENO)) {
    return false;
  }

#ifdef _WIN32
  // under Windows the readline is not compilable
  return false;
#elif defined TRI_HAVE_LINENOISE
  return false;
#elif defined TRI_HAVE_READLINE
  return true;
#else
  return false;
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
