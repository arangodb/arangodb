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

#include "ScriptLoader.h"
#include "Basics/MutexLocker.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/StringUtils.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

ScriptLoader::ScriptLoader() : _scripts(), _directory(), _lock() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

std::string const& ScriptLoader::getDirectory() const { return _directory; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the directory for scripts
////////////////////////////////////////////////////////////////////////////////

void ScriptLoader::setDirectory(std::string const& directory) {
  MUTEX_LOCKER(mutexLocker, _lock);

  _directory = directory;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build a script from an array of strings
////////////////////////////////////////////////////////////////////////////////

std::string ScriptLoader::buildScript(char const** script) {
  std::string scriptString;

  while (true) {
    std::string tempStr = std::string(*script);

    if (tempStr == "//__end__") {
      break;
    }

    scriptString += tempStr + "\n";

    ++script;
  }

  return scriptString;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new named script
////////////////////////////////////////////////////////////////////////////////

void ScriptLoader::defineScript(std::string const& name,
                                std::string const& script) {
  MUTEX_LOCKER(mutexLocker, _lock);

  _scripts[name] = script;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new named script
////////////////////////////////////////////////////////////////////////////////

void ScriptLoader::defineScript(std::string const& name, char const** script) {
  std::string scriptString = buildScript(script);

  MUTEX_LOCKER(mutexLocker, _lock);

  _scripts[name] = scriptString;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a named script
////////////////////////////////////////////////////////////////////////////////

std::string const& ScriptLoader::findScript(std::string const& name) {
  static std::string empty = "";

  MUTEX_LOCKER(mutexLocker, _lock);

  std::map<std::string, std::string>::iterator i = _scripts.find(name);

  if (i != _scripts.end()) {
    return i->second;
  }

  if (!_directory.empty()) {
    std::vector<std::string> parts = getDirectoryParts();

    for (size_t i = 0; i < parts.size(); i++) {
      char* filename = TRI_Concatenate2File(parts.at(i).c_str(), name.c_str());
      char* result = TRI_SlurpFile(TRI_CORE_MEM_ZONE, filename, nullptr);

      if (result == nullptr && (i == parts.size() - 1)) {
        LOG(ERR) << "cannot locate file '" << StringUtils::correctPath(name) << "': " << TRI_last_error();
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      if (result != nullptr) {
        _scripts[name] = result;
        TRI_FreeString(TRI_CORE_MEM_ZONE, result);
        return _scripts[name];
      }
    }
  }

  return empty;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a list of all specified directory parts
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ScriptLoader::getDirectoryParts() {
  std::vector<std::string> directories;

  if (!_directory.empty()) {
// .........................................................................
// for backwards compatibility allow ":" as a delimiter for POSIX like
// implementations, otherwise we will only allow ";"
// .........................................................................

#ifdef _WIN32
    TRI_vector_string_t parts = TRI_Split2String(_directory.c_str(), ";");
#else
    TRI_vector_string_t parts = TRI_Split2String(_directory.c_str(), ":;");
#endif

    for (size_t i = 0; i < parts._length; i++) {
      std::string part = StringUtils::trim(parts._buffer[i]);

      if (!part.empty()) {
        directories.push_back(part);
      }
    }

    TRI_DestroyVectorString(&parts);
  }

  return directories;
}
