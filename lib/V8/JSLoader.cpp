////////////////////////////////////////////////////////////////////////////////
/// @brief source code loader
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

#include "JSLoader.h"

#include "Basics/MutexLocker.h"
#include "BasicsC/files.h"
#include "BasicsC/strings.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

JSLoader::JSLoader () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a named script in the global context
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> JSLoader::executeGlobalScript (v8::Persistent<v8::Context> context,
                                                     string const& name) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  findScript(name);

  map<string, string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    // correct the path/name
    LOGGER_ERROR("unknown script '" << StringUtils::correctPath(name) << "'");
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Value> result = TRI_ExecuteJavaScriptString(context,
                                                             v8::String::New(i->second.c_str()),
                                                             v8::String::New(name.c_str()),
                                                             false);

  if (tryCatch.HasCaught()) {
    TRI_LogV8Exception(&tryCatch);
    return scope.Close(v8::Undefined());
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a named script
////////////////////////////////////////////////////////////////////////////////

bool JSLoader::loadScript (v8::Persistent<v8::Context> context, string const& name) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  findScript(name);

  map<string, string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    // correct the path/name
    LOGGER_ERROR("unknown script '" << StringUtils::correctPath(name) << "'");
    return false;
  }

  TRI_ExecuteJavaScriptString(context,
                              v8::String::New(i->second.c_str()),
                              v8::String::New(name.c_str()),
                              false);

  if (tryCatch.HasCaught()) {
    TRI_LogV8Exception(&tryCatch);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads all scripts
////////////////////////////////////////////////////////////////////////////////

bool JSLoader::loadAllScripts (v8::Persistent<v8::Context> context) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (_directory.empty()) {
    return true;
  }

  vector<string> parts = getDirectoryParts();

  bool result = true;
  for (size_t i = 0; i < parts.size(); i++) {
    result &= TRI_ExecuteGlobalJavaScriptDirectory(parts.at(i).c_str());
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a named script
////////////////////////////////////////////////////////////////////////////////

bool JSLoader::executeScript (v8::Persistent<v8::Context> context, string const& name) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  findScript(name);

  map<string, string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    return false;
  }

  string content = "(function() { " + i->second + "/* end-of-file '" + name + "' */ })()";

  TRI_ExecuteJavaScriptString(context,
                              v8::String::New(content.c_str()),
                              v8::String::New(name.c_str()),
                              false);

  if (! tryCatch.HasCaught()) {
    TRI_LogV8Exception(&tryCatch);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all scripts
////////////////////////////////////////////////////////////////////////////////

bool JSLoader::executeAllScripts (v8::Persistent<v8::Context> context) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;
  bool ok;

  if (_directory.empty()) {
    return true;
  }

  ok = TRI_ExecuteLocalJavaScriptDirectory(_directory.c_str());

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
