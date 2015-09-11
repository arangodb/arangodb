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

#ifndef ARANGODB_UTILITIES_SHELL_IMPL_FACTORY_H
#define ARANGODB_UTILITIES_SHELL_IMPL_FACTORY_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace arangodb {
  class ShellImplementation;
  class Completer;

// -----------------------------------------------------------------------------
// --SECTION--                                            class ShellImplFactory
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ShellImplFactory
////////////////////////////////////////////////////////////////////////////////

  class ShellImplFactory {
    ShellImplFactory () = delete;
    ShellImplFactory (const ShellImplFactory&) = delete;
    ShellImplFactory& operator= (const ShellImplFactory&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

     public:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shell
////////////////////////////////////////////////////////////////////////////////

      static ShellImplementation* buildShell (std::string const& history,
					      Completer*);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the shell will have a CTRL-C handler
////////////////////////////////////////////////////////////////////////////////

      static bool hasCtrlCHandler ();
  };
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
