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

#include "JSLoader.h"

#include "Basics/MutexLocker.h"
#include "Basics/files.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/StringUtils.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

JSLoader::JSLoader () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a named script in the global context
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> JSLoader::executeGlobalScript (v8::Handle<v8::Context> context,
                                                     string const& name) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  findScript(name);

  map<string, string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    // correct the path/name
    LOG_ERROR("unknown script '%s'", StringUtils::correctPath(name).c_str());
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Value> result = TRI_ExecuteJavaScriptString(context,
                                                             v8::String::New(i->second.c_str(), (int) i->second.size()),
                                                             v8::String::New(name.c_str(), (int) name.size()),
                                                             false);

  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      TRI_LogV8Exception(&tryCatch);
      return scope.Close(v8::Undefined());
    }
    else {
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

      v8g->_canceled = true;
      return scope.Close(result);
    }
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
    LOG_ERROR("unknown script '%s'", StringUtils::correctPath(name).c_str());
    return false;
  }

  TRI_ExecuteJavaScriptString(context,
                              v8::String::New(i->second.c_str(), (int) i->second.size()),
                              v8::String::New(name.c_str(), (int) name.size()),
                              false);

  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      TRI_LogV8Exception(&tryCatch);
      return false;
    }
    else {
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

      v8g->_canceled = true;
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads all scripts
////////////////////////////////////////////////////////////////////////////////

bool JSLoader::loadAllScripts (v8::Persistent<v8::Context> context) {
  v8::HandleScope scope;

  if (_directory.empty()) {
    return true;
  }

  vector<string> parts = getDirectoryParts();

  bool result = true;

  for (size_t i = 0; i < parts.size(); i++) {
    result = result && TRI_ExecuteGlobalJavaScriptDirectory(parts.at(i).c_str());
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
                              v8::String::New(content.c_str(), (int) content.size()),
                              v8::String::New(name.c_str(), (int) name.size()),
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
