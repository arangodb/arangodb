////////////////////////////////////////////////////////////////////////////////
/// @brief abstract line editor
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILITIES_LINE_EDITOR_H
#define TRIAGENS_UTILITIES_LINE_EDITOR_H 1

#include "Basics/Common.h"
#include "LineEditor.h"
#include "Completer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  class LineEditor
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor
////////////////////////////////////////////////////////////////////////////////
namespace triagens {
  class ShellImplementation;
 
  class LineEditor {
    LineEditor(LineEditor const&);
    LineEditor& operator= (LineEditor const&);

    // -----------------------------------------------------------------------------
    // --SECTION--                                                  public constants
    // -----------------------------------------------------------------------------

  public:

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of history entries
    ////////////////////////////////////////////////////////////////////////////////

    static const int MAX_HISTORY_ENTRIES = 1000;

    // -----------------------------------------------------------------------------
    // --SECTION--                                      constructors and destructors
    // -----------------------------------------------------------------------------

  public:

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief constructor
    ////////////////////////////////////////////////////////////////////////////////

    LineEditor(std::string const& history);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief destructor
    ////////////////////////////////////////////////////////////////////////////////

    virtual ~LineEditor();

    // -----------------------------------------------------------------------------
    // --SECTION--                                                    public methods
    // -----------------------------------------------------------------------------

  public:

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief line editor open
    ////////////////////////////////////////////////////////////////////////////////

    bool open(bool autoComplete);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief line editor shutdown
    ////////////////////////////////////////////////////////////////////////////////

    bool close();

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief line editor prompt
    ////////////////////////////////////////////////////////////////////////////////

    char* prompt(char const*);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief get the history file path
    ///
    /// The path is "$HOME" plus _historyFilename, if $HOME is set. Else
    /// the local file _historyFilename.
    ////////////////////////////////////////////////////////////////////////////////

    std::string historyPath();

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief add to history
    ////////////////////////////////////////////////////////////////////////////////

    void addHistory(const char*);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief save the history
    ////////////////////////////////////////////////////////////////////////////////

    bool writeHistory();

    ////////////////////////////////////////////////////////////////////////////////
    /// @}
    ////////////////////////////////////////////////////////////////////////////////

    // -----------------------------------------------------------------------------
    // --SECTION--                                                 protected methods
    // -----------------------------------------------------------------------------
  
  protected:

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief      arranges for the correct creation of the the ShellImplementation
    ////////////////////////////////////////////////////////////////////////////////

    void prepareShell();

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief      creates a concrete Shell with the correct parameter (Completer!!)
    ////////////////////////////////////////////////////////////////////////////////

    virtual void initializeShell() = 0;

  protected:

    // -----------------------------------------------------------------------------
    // --SECTION--                                               protected variables
    // -----------------------------------------------------------------------------
    ShellImplementation * _shellImpl;

    string _history;
  };
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

