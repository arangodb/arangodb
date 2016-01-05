////////////////////////////////////////////////////////////////////////////////
/// @brief base class for a line editor
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILITIES_LINE_EDITOR_H
#define ARANGODB_UTILITIES_LINE_EDITOR_H 1

#include "Basics/Common.h"
#include <functional>

namespace arangodb {
class ShellBase;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class LineEditor
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor
////////////////////////////////////////////////////////////////////////////////

class LineEditor {
  LineEditor(LineEditor const&) = delete;
  LineEditor& operator=(LineEditor const&) = delete;

  // -----------------------------------------------------------------------------
  // --SECTION--                                                  public
  // constants
  // -----------------------------------------------------------------------------

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief number of history entries
  ////////////////////////////////////////////////////////////////////////////////

  static const int MAX_HISTORY_ENTRIES = 1000;

  // -----------------------------------------------------------------------------
  // --SECTION--                                      constructors and
  // destructors
  // -----------------------------------------------------------------------------

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief constructor
  ////////////////////////////////////////////////////////////////////////////////

  LineEditor();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief destructor
  ////////////////////////////////////////////////////////////////////////////////

  virtual ~LineEditor();

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    public
  // methods
  // -----------------------------------------------------------------------------

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the shell implementation supports colors
  ////////////////////////////////////////////////////////////////////////////////

  bool supportsColors() const;

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

  std::string prompt(const std::string& prompt, const std::string& begin,
                     bool& eof);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief add to history
  ////////////////////////////////////////////////////////////////////////////////

  void addHistory(const std::string& line);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief send a signal to the shell implementation
  ////////////////////////////////////////////////////////////////////////////////

  void signal();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief register a callback function to be executed on signal receipt
  ////////////////////////////////////////////////////////////////////////////////

  void setSignalFunction(std::function<void()> const&);

  // -----------------------------------------------------------------------------
  // --SECTION--                                               protected
  // variables
  // -----------------------------------------------------------------------------

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the shell implementation
  ////////////////////////////////////////////////////////////////////////////////

  ShellBase* _shell;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief callback function to be executed on signal receipt
  ////////////////////////////////////////////////////////////////////////////////

  std::function<void()> _signalFunc;
};
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
