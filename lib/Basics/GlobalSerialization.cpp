////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/GlobalSerialization.h"

#include <thread>

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"

namespace arangodb {

void waitForGlobalEvent(std::string_view id, std::string_view selector) {
  std::vector<std::string> progLines;
  // Read program:
  try {
    std::string prog = arangodb::basics::FileUtils::slurp("/tmp/debuggingSLP");
    progLines = arangodb::basics::StringUtils::split(prog, '\n');
  } catch (std::exception const&) {
    return;  // No program, no waiting
  }
  TRI_ASSERT(!progLines.empty());
  // Check if we are affected:
  bool affected = false;
  for (size_t i = 0; i < progLines.size(); ++i) {
    std::vector<std::string> parts =
        arangodb::basics::StringUtils::split(progLines[i], ' ');
    if (parts.size() >= 2 && parts[0] == id && parts[1] == selector) {
      affected = true;
      break;
    }
  }
  if (!affected) {
    return;
  }
  LOG_DEVEL << "Waiting for global event " << id << " with selector "
            << selector << "...";
  while (true) {
    // Read current line:
    std::string current;
    try {
      current = arangodb::basics::FileUtils::slurp("/tmp/debuggingSLPCurrent");
    } catch (std::exception const&) {
    }
    LOG_DEVEL << "Waiting for global event " << id << " with selector "
              << selector << "..." << current;
    if (!current.empty()) {
      // Protect against the case that the file might not be there currently,
      // this also covers the case that the file exists but is empty.
      uint64_t line = arangodb::basics::StringUtils::uint64(current);
      if (line >= progLines.size()) {
        return;
      }
      std::vector<std::string> parts =
          arangodb::basics::StringUtils::split(progLines[line], ' ');
      if (parts.size() >= 2 && id == parts[0] && selector == parts[1]) {
        // Hurray! We can make progress:
        if (parts.size() >= 3) {
          LOG_DEVEL << "Global event " << id << " with selector " << selector
                    << " and comment " << parts[2] << " has happened...";
        } else {
          LOG_DEVEL << "Global event " << id << " with selector " << selector
                    << " has happened...";
        }
        arangodb::basics::FileUtils::spit("/tmp/debuggingSLPCurrent",
                                          std::to_string(line + 1));
        return;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
}

}  // namespace arangodb
