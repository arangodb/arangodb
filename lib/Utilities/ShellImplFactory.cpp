////////////////////////////////////////////////////////////////////////////////
/// @brief a basis class which defines the methods for determining
///        when an input is "complete"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ShellImplFactory.h"

#ifdef _WIN32
#include "LinenoiseShell.h"
#elif defined TRI_HAVE_LINENOISE
#include "LinenoiseShell.h"
#elif defined TRI_HAVE_READLINE
#include "ReadlineShell.h"
#else 
#include "DummyShell.h"
#endif

using namespace triagens;
using namespace std;

ShellImplementation* ShellImplFactory::buildShell (string const & history, 
                                                   Completer* completer) {

#ifdef _WIN32
  //under windows the readline is not compilable
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
