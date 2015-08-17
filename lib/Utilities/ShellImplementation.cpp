////////////////////////////////////////////////////////////////////////////////
/// @brief implementation of the base class ShellImplementation
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

#include "ShellImplementation.h"

#include <iostream>

#include "Basics/StringUtils.h"
#include "Utilities/Completer.h"

using namespace std;
using namespace arangodb;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                         class ShellImplementation
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ShellImplementation::ShellImplementation (string const& history, 
                                          Completer* completer)
  : _current(),
    _historyFilename(history),
    _state(STATE_NONE),
    _completer(completer) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ShellImplementation::~ShellImplementation () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a signal
////////////////////////////////////////////////////////////////////////////////

void ShellImplementation::signal () {
  // do nothing special
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor prompt
////////////////////////////////////////////////////////////////////////////////

string ShellImplementation::prompt (const string& prompt,
				    const string& plain,
				    bool& eof) {
  size_t lineno = 0;
  string dotdot = "...> ";
  string p = prompt;
  string sep = "";
  string line;

  eof = false;

  while (true) {

    // calling concrete implmentation of the shell
    line = getLine(p, eof);
    p = dotdot;

    if (eof) {

      // give up, if the user pressed control-D on the top-most level
      if (_current.empty()) {
        return "";
      }

      // otherwise clear current content
      _current.clear();
      eof = false;
      break;
    }

    _current += sep;
    sep = "\n";
    ++lineno;

    // remove any prompt at the beginning of the line
    size_t pos = string::npos;

    if (StringUtils::isPrefix(line, plain)) {
      pos = line.find('>');
    }
    else if (StringUtils::isPrefix(line, "...")) {
      pos = line.find('>');
    }

    if (pos != string::npos) {
      pos = line.find_first_not_of(" \t", pos + 1);

      if (pos != string::npos) {
	line = line.substr(pos);
      }
      else {
	line.clear();
      }
    }

    // extend line and check
    _current += line;

    // stop if line is complete
    bool ok = _completer->isComplete(_current, lineno);

    if (ok) {
      break;
    }
  }

  // clear current line
  line = _current;
  _current.clear();

  return line;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
