////////////////////////////////////////////////////////////////////////////////
/// @brief source code loader
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

#ifndef ARANGODB_V8_JSLOADER_H
#define ARANGODB_V8_JSLOADER_H 1

#include "Basics/Common.h"

#include "Utilities/ScriptLoader.h"

#include <v8.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                    class JSLoader
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript source code loader
////////////////////////////////////////////////////////////////////////////////

    class JSLoader : public ScriptLoader {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

        JSLoader ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a named script in the global context
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> executeGlobalScript (v8::Handle<v8::Context> context,
                                                   std::string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a named script
////////////////////////////////////////////////////////////////////////////////

        bool loadScript (v8::Persistent<v8::Context>, std::string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief loads all scripts
////////////////////////////////////////////////////////////////////////////////

        bool loadAllScripts (v8::Persistent<v8::Context>);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a named script
////////////////////////////////////////////////////////////////////////////////

        bool executeScript (v8::Persistent<v8::Context>, std::string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all scripts
////////////////////////////////////////////////////////////////////////////////

        bool executeAllScripts (v8::Persistent<v8::Context>);
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
