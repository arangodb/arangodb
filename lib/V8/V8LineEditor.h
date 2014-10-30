////////////////////////////////////////////////////////////////////////////////
/// @brief V8 line editor
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8_V8LINE_EDITOR_H
#define ARANGODB_V8_V8LINE_EDITOR_H 1

#include "Basics/Common.h"

#include "Utilities/LineEditor.h"
#include "Utilities/Completer.h"

#include <v8.h>

using namespace triagens;

// -----------------------------------------------------------------------------
// --SECTION--                                                 class V8Completer
// -----------------------------------------------------------------------------

class V8Completer : public Completer {

  enum {
    NORMAL,             // start
    NORMAL_1,           // from NORMAL: seen a single /
    DOUBLE_QUOTE,       // from NORMAL: seen a single "
    DOUBLE_QUOTE_ESC,   // from DOUBLE_QUOTE: seen a backslash
    SINGLE_QUOTE,       // from NORMAL: seen a single '
    SINGLE_QUOTE_ESC,   // from SINGLE_QUOTE: seen a backslash
    MULTI_COMMENT,      // from NORMAL_1: seen a *
    MULTI_COMMENT_1,    // from MULTI_COMMENT, seen a *
    SINGLE_COMMENT      // from NORMAL_1; seen a /
  }
  state;

  virtual bool isComplete (std::string const&, 
                           size_t lineno, 
                           size_t column);

  virtual void getAlternatives (char const*, 
                                std::vector<std::string>&);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                class V8LineEditor
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor
////////////////////////////////////////////////////////////////////////////////

class V8LineEditor : public LineEditor {

  V8LineEditor (LineEditor const&) = delete;
  V8LineEditor& operator= (LineEditor const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    V8LineEditor (v8::Handle<v8::Context>, std::string const& history);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    ~V8LineEditor ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief     creates a concrete Shell with the correct parameter (Completer!!)
////////////////////////////////////////////////////////////////////////////////

  protected:

    virtual void initializeShell();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief context
////////////////////////////////////////////////////////////////////////////////

    v8::Handle<v8::Context> _context;

    V8Completer _completer;
};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
