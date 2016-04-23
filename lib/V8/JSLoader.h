////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8_JSLOADER_H
#define ARANGODB_V8_JSLOADER_H 1

#include "Basics/Common.h"
#include "Utilities/ScriptLoader.h"

#include <v8.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript source code loader
////////////////////////////////////////////////////////////////////////////////

class JSLoader : public ScriptLoader {
 public:
  enum eState { eFailLoad, eFailExecute, eSuccess };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a loader
  //////////////////////////////////////////////////////////////////////////////

  JSLoader();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief executes a named script in the global context
  //////////////////////////////////////////////////////////////////////////////

  v8::Handle<v8::Value> executeGlobalScript(v8::Isolate* isolate,
                                            v8::Handle<v8::Context> context,
                                            std::string const& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief loads a named script
  //////////////////////////////////////////////////////////////////////////////

  JSLoader::eState loadScript(v8::Isolate* isolate, v8::Handle<v8::Context>&,
                              std::string const& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief loads all scripts
  //////////////////////////////////////////////////////////////////////////////

  bool loadAllScripts(v8::Isolate* isolate, v8::Handle<v8::Context>& context);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief executes a named script
  //////////////////////////////////////////////////////////////////////////////

  bool executeScript(v8::Isolate* isolate, v8::Handle<v8::Context>& context,
                     std::string const& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief executes all scripts
  //////////////////////////////////////////////////////////////////////////////

  bool executeAllScripts(v8::Isolate* isolate,
                         v8::Handle<v8::Context>& context);
};
}

#endif
