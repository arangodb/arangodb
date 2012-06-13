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

#include "ScriptLoader.h"

#include "Basics/MutexLocker.h"
#include "BasicsC/files.h"
#include "BasicsC/strings.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

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

ScriptLoader::ScriptLoader ()
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
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

string const& ScriptLoader::getDirectory () const {
  return _directory;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

void ScriptLoader::setDirectory (string const& directory) {
  MUTEX_LOCKER(_lock);

  _directory = directory;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new named script
////////////////////////////////////////////////////////////////////////////////

void ScriptLoader::defineScript (string const& name, string const& script) {
  MUTEX_LOCKER(_lock);

  _scripts[name] = script;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a named script
////////////////////////////////////////////////////////////////////////////////

string const& ScriptLoader::findScript (string const& name) {
  MUTEX_LOCKER(_lock);
  static string empty = "";

  map<string, string>::iterator i = _scripts.find(name);

  if (i != _scripts.end()) {
    return i->second;
  }

  if (! _directory.empty()) {
    vector<string> parts = getDirectoryParts();

    for (size_t i = 0; i < parts.size(); i++) {
      char* filename = TRI_Concatenate2File(parts.at(i).c_str(), name.c_str());
      char* result = TRI_SlurpFile(TRI_CORE_MEM_ZONE, filename);

      if (result == 0 && (i == parts.size() - 1)) {
        LOGGER_ERROR << "cannot locate file '" << name.c_str() << "': " << TRI_last_error();
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      if (result != 0) {
        _scripts[name] = result;
        TRI_FreeString(TRI_CORE_MEM_ZONE, result);
        return _scripts[name];
      }
    }
  }

  return empty;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a list of all specified directory parts
////////////////////////////////////////////////////////////////////////////////

vector<string> ScriptLoader::getDirectoryParts () {
  vector<string> directories;
  
  if (! _directory.empty()) {
    TRI_vector_string_t parts = TRI_Split2String(_directory.c_str(), ":;");

    for (size_t i = 0; i < parts._length; i++) {
      string part = StringUtils::trim(parts._buffer[i]);

      if (! part.empty()) {
        directories.push_back(part);
      }
    }

    TRI_DestroyVectorString(&parts);
  }
  
  return directories;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
