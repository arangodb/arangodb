////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Basics/FileUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <cstddef>
#include <utility>

using namespace arangodb;
using namespace arangodb::basics;

/// @brief sets the directory for scripts
void ScriptLoader::setDirectory(std::string const& directory) {
  std::lock_guard mutexLocker{_lock};

  _directory = directory;
}

/// @brief finds a named script
std::string const& ScriptLoader::findScript(std::string const& name) {
  std::lock_guard mutexLocker{_lock};

  auto it = _scripts.find(name);

  if (it != _scripts.end()) {
    return it->second;
  }

  if (!_directory.empty()) {
    std::vector<std::string> parts = getDirectoryParts();

    for (size_t i = 0; i < parts.size(); i++) {
      std::string filename = basics::FileUtils::buildFilename(parts[i], name);
      char* result = TRI_SlurpFile(filename.c_str(), nullptr);

      if (result == nullptr && (i == parts.size() - 1)) {
        LOG_TOPIC("8d6a7", ERR, arangodb::Logger::FIXME)
            << "cannot locate file '" << StringUtils::correctPath(name)
            << "', path: '" << parts[i] << "': " << TRI_last_error();
      }

      if (result != nullptr) {
        _scripts[name] = result;
        TRI_FreeString(result);
        return _scripts[name];
      }
    }
  }

  return StaticStrings::Empty;
}

/// @brief gets a list of all specified directory parts
std::vector<std::string> ScriptLoader::getDirectoryParts() {
  std::vector<std::string> directories;

  if (!_directory.empty()) {
    // .........................................................................
    // for backwards compatibility allow ":" as a delimiter for POSIX like
    // implementations, otherwise we will only allow ";"
    // .........................................................................

    std::vector<std::string> parts =
        basics::StringUtils::split(_directory, ":;");

    for (size_t i = 0; i < parts.size(); ++i) {
      std::string part = StringUtils::trim(parts[i]);

      if (!part.empty()) {
        directories.push_back(std::move(part));
      }
    }
  }

  return directories;
}
