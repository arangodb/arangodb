////////////////////////////////////////////////////////////////////////////////
/// @brief source code loader
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "JSLoader.h"

#include "Basics/MutexLocker.h"
#include "BasicsC/files.h"
#include "BasicsC/strings.h"
#include "Logger/Logger.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::avocado;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

JSLoader::JSLoader ()
  : _scripts(),
    _directory(),
    _lock() {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

string const& JSLoader::getDirectory () const {
  return _directory;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

void JSLoader::setDirectory (string const& directory) {
  MUTEX_LOCKER(_lock);

  _directory = directory;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new named script
////////////////////////////////////////////////////////////////////////////////

void JSLoader::defineScript (string const& name, string const& script) {
  MUTEX_LOCKER(_lock);

  _scripts[name] = script;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a named script
////////////////////////////////////////////////////////////////////////////////

string const& JSLoader::findScript (string const& name) {
  MUTEX_LOCKER(_lock);
  static string empty = "";

  map<string, string>::iterator i = _scripts.find(name);

  if (i != _scripts.end()) {
    return i->second;
  }

  if (! _directory.empty()) {
    char* filename = TRI_Concatenate2File(_directory.c_str(), name.c_str());
    char* result = TRI_SlurpFile(filename);

    if (result == 0) {
      LOGGER_ERROR << "cannot load file '" << filename << "': " << TRI_last_error();
    }

    TRI_FreeString(filename);

    if (result != 0) {
      _scripts[name] = result;
      TRI_FreeString(result);
      return _scripts[name];
    }
  }

  return empty;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a named script
////////////////////////////////////////////////////////////////////////////////

bool JSLoader::loadScript (v8::Persistent<v8::Context> context, string const& name) {
  v8::HandleScope scope;

  findScript(name);

  map<string, string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    return false;
  }

  return TRI_ExecuteStringVocBase(context,
                                  v8::String::New(i->second.c_str()),
                                  v8::String::New(name.c_str()),
                                  false,
                                  true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads all scripts
////////////////////////////////////////////////////////////////////////////////

bool JSLoader::loadAllScripts (v8::Persistent<v8::Context> context) {
  if (_directory.empty()) {
    return true;
  }

  return TRI_LoadJavaScriptDirectory(context, _directory.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
