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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/GlobalSerialization.h"

#include <mutex>
#include <thread>

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"

namespace arangodb {

namespace {
std::pair<std::string, std::string> findSLPProgramPaths() {
  std::string path = "/tmp";
  TRI_GETENV("ARANGOTEST_ROOT_DIR", path);
  std::string progPath =
      basics::FileUtils::buildFilename(path.c_str(), "globalSLP");
  std::string pcPath =
      basics::FileUtils::buildFilename(path.c_str(), "globalSLP_PC");
  return std::pair(progPath, pcPath);
}

std::vector<std::string> readSLPProgram(std::string const& progPath) {
  // Read program:
  std::vector<std::string> progLines;
  try {
    std::string prog = arangodb::basics::FileUtils::slurp(progPath);
    if (prog.empty()) {
      return progLines;
    }
    progLines = arangodb::basics::StringUtils::split(prog, '\n');
    progLines.pop_back();  // Ignore last line, since every line is
                           // ended by \n.
  } catch (std::exception const&) {
    // No program, return empty
  }
  return progLines;
}

std::mutex globalSLPModificationMutex;

}  // namespace

void waitForGlobalEvent(std::string_view id, std::string_view selector) {
  auto [progPath, pcPath] = findSLPProgramPaths();
  std::vector<std::string> progLines = readSLPProgram(progPath);
  if (progLines.empty()) {
    return;
  }
  LOG_TOPIC("ace32", INFO, Logger::MAINTENANCE)
      << "Waiting for global event " << id << " with selector " << selector
      << "...";
  while (true) {
    // Read pc
    std::vector<std::string> executedLines = readSLPProgram(pcPath);
    if (executedLines.size() >= progLines.size()) {
      return;  // program already finished
    }
    std::string current = progLines[executedLines.size()];
    std::vector<std::string> parts =
        arangodb::basics::StringUtils::split(current, ' ');
    if (parts.size() >= 2 && id == parts[0] && selector == parts[1]) {
      // Hurray! We can make progress:
      if (parts.size() >= 3) {
        LOG_TOPIC("ace33", INFO, Logger::MAINTENANCE)
            << "Global event " << id << " with selector " << selector
            << " and comment " << parts[2] << " has happened...";
      } else {
        LOG_TOPIC("ace34", INFO, Logger::MAINTENANCE)
            << "Global event " << id << " with selector " << selector
            << " has happened...";
      }
      current.push_back('\n');
      try {
        std::lock_guard<std::mutex> guard(globalSLPModificationMutex);
        arangodb::basics::FileUtils::appendToFile(pcPath, current);
      } catch (std::exception const&) {
        // ignore
      }
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
}

void observeGlobalEvent(std::string_view id, std::string_view selector) {
  auto [progPath, pcPath] = findSLPProgramPaths();
  std::vector<std::string> progLines = readSLPProgram(progPath);
  if (progLines.empty()) {
    return;
  }
  std::string path = TRI_GetTempPath();
  LOG_TOPIC("ace35", INFO, Logger::MAINTENANCE)
      << "Observing global event " << id << " with selector " << selector
      << "...";
  // Read pc
  std::vector<std::string> executedLines = readSLPProgram(pcPath);
  if (executedLines.size() >= progLines.size()) {
    LOG_TOPIC("ace38", DEBUG, Logger::MAINTENANCE)
        << "SLP has already finished";
    return;  // program already finished
  }
  std::string current = progLines[executedLines.size()];
  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(current, ' ');
  if (parts.size() >= 2 && id == parts[0] && selector == parts[1]) {
    // Hurray! We can make progress:
    if (parts.size() >= 3) {
      LOG_TOPIC("ace36", INFO, Logger::MAINTENANCE)
          << "Global event " << id << " with selector " << selector
          << " and comment " << parts[2] << " was observed...";
    } else {
      LOG_TOPIC("ace37", INFO, Logger::MAINTENANCE)
          << "Global event " << id << " with selector " << selector
          << " was observed...";
    }
    current.push_back('\n');
    try {
      std::lock_guard<std::mutex> guard(globalSLPModificationMutex);
      arangodb::basics::FileUtils::appendToFile(pcPath, current);
    } catch (std::exception const&) {
      // ignore
    }
  }
}

}  // namespace arangodb
