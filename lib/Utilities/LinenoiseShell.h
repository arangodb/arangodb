////////////////////////////////////////////////////////////////////////////////
/// @brief a basis class for concrete implementations for a shell
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

#ifndef ARANGODB_UTILITIES_LINENOISE_SHELL_H
#define ARANGODB_UTILITIES_LINENOISE_SHELL_H 1

#include "Basics/Common.h"

#include "Completer.h"
#include "ShellImplementation.h"

#include "BasicsC/tri-strings.h"
#include "V8/v8-utils.h"

namespace triagens {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

  class LinenoiseShell: public ShellImplementation {

  public:
////////////////////////////////////////////////////////////////////////////////
///                                               public constructor, destructor
////////////////////////////////////////////////////////////////////////////////

    LinenoiseShell(std::string const& history, Completer *);

    virtual ~LinenoiseShell();

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

    virtual bool open(bool autoComplete);

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor shutdown
////////////////////////////////////////////////////////////////////////////////

    virtual bool close();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the history file path
///
/// The path is "$HOME" plus _historyFilename, if $HOME is set. Else
/// the local file _historyFilename.
////////////////////////////////////////////////////////////////////////////////

    virtual std::string historyPath();

////////////////////////////////////////////////////////////////////////////////
/// @brief add to history
////////////////////////////////////////////////////////////////////////////////

    virtual void addHistory(const char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief save the history
////////////////////////////////////////////////////////////////////////////////

    virtual bool writeHistory();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the characters which the user has typed
/// @arg  is the prompt of the shell
/// Note: this is the interface between our shell world and some implementation
///       of key events (linenoise, readline)
////////////////////////////////////////////////////////////////////////////////

    virtual char* getLine (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
