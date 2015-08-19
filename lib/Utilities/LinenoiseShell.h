////////////////////////////////////////////////////////////////////////////////
/// @brief console input using linenoise
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_UTILITIES_LINENOISE_SHELL_H
#define ARANGODB_UTILITIES_LINENOISE_SHELL_H 1

#include "ShellImplementation.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace arangodb {
  class Completer;

// -----------------------------------------------------------------------------
// --SECTION--                                              class LinenoiseShell
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief LinenoiseShell
////////////////////////////////////////////////////////////////////////////////

  class LinenoiseShell : public ShellImplementation {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    LinenoiseShell (std::string const& history, Completer*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    virtual ~LinenoiseShell ();

// -----------------------------------------------------------------------------
// --SECTION--                                       ShellImplementation methods
// -----------------------------------------------------------------------------

    public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

      bool open (bool autoComplete) override final;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

      bool close () override final;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

      std::string historyPath() override final;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

      void addHistory (const std::string&) override final;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

      bool writeHistory () override final;

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

      std::string getLine (const std::string& prompt, bool& eof) override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the shell implementation supports colors
////////////////////////////////////////////////////////////////////////////////

      bool supportsColors () override final {
        return false;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the shell supports a CTRL-C handler
////////////////////////////////////////////////////////////////////////////////

      bool supportsCtrlCHandler () override final {
        return false;
      }
  };
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
