////////////////////////////////////////////////////////////////////////////////
/// @brief a base class for implementations for a shell
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
/// @author Dr. Frank Celler
/// @author Esteban Lombeyda
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILITIES_SHELL_IMPLEMENTATION_H
#define ARANGODB_UTILITIES_SHELL_IMPLEMENTATION_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace arangodb {
  class Completer;

// -----------------------------------------------------------------------------
// --SECTION--                                         class ShellImplementation
// -----------------------------------------------------------------------------

  class ShellImplementation {

// -----------------------------------------------------------------------------
// --SECTION--                                                   protected types
// -----------------------------------------------------------------------------

  protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief state of the console
////////////////////////////////////////////////////////////////////////////////

    enum console_state_e {
      STATE_NONE = 0,
      STATE_OPENED = 1,
      STATE_CLOSED = 2
    };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    ShellImplementation (std::string const& history, Completer*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    virtual ~ShellImplementation ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor prompt
////////////////////////////////////////////////////////////////////////////////

    std::string prompt (const std::string& prompt,
			const std::string& begin,
			bool& eof);

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a signal
////////////////////////////////////////////////////////////////////////////////

    virtual void signal ();

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

    virtual bool open (bool autoComplete) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor shutdown
////////////////////////////////////////////////////////////////////////////////

    virtual bool close () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the history file path
///
/// The path is "$HOME" plus _historyFilename, if $HOME is set. Else
/// the local file _historyFilename.
////////////////////////////////////////////////////////////////////////////////

    virtual std::string historyPath () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief add to history
////////////////////////////////////////////////////////////////////////////////

    virtual void addHistory (const std::string&) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief save the history
////////////////////////////////////////////////////////////////////////////////

    virtual bool writeHistory () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief get next line
////////////////////////////////////////////////////////////////////////////////

    virtual std::string getLine (const std::string& prompt, bool& eof) = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

  protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief current text
////////////////////////////////////////////////////////////////////////////////

    std::string _current;

////////////////////////////////////////////////////////////////////////////////
/// @brief history filename
////////////////////////////////////////////////////////////////////////////////

    std::string _historyFilename;

////////////////////////////////////////////////////////////////////////////////
/// @brief current console state
////////////////////////////////////////////////////////////////////////////////

    console_state_e _state;

////////////////////////////////////////////////////////////////////////////////
/// @brief object which defines when the input is finished
////////////////////////////////////////////////////////////////////////////////

    Completer* _completer;

  };
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
