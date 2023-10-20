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
#include "Basics/files.h"
#include "Logger/LogMacros.h"

namespace arangodb {

void waitForGlobalEvent(std::string_view id, std::string_view selector) {
  std::string path = TRI_GetTempPath();
  auto pos = path.rfind(TRI_DIR_SEPARATOR_CHAR);
  if (pos != std::string::npos) {
    path = path.substr(0, pos);
  }
  std::string progPath =
      basics::FileUtils::buildFilename(path.c_str(), "globalSLP");
  std::string pcPath =
      basics::FileUtils::buildFilename(path.c_str(), "globalSLP_PC");

  std::vector<std::string> progLines;
  // Read program:
  try {
    std::string prog = arangodb::basics::FileUtils::slurp(progPath);
    if (prog.empty()) {
      return;
    }
    progLines = arangodb::basics::StringUtils::split(prog, '\n');
    progLines.pop_back();  // Ignore last line, since every line is
                           // ended by \n.
    if (progLines.empty()) {
      return;
    }
  } catch (std::exception const&) {
    return;  // No program, no waiting
  }
  LOG_TOPIC("ace32", INFO, Logger::MAINTENANCE)
      << "Waiting for global event " << id << " with selector " << selector
      << "...";
  while (true) {
    // Read pc
    std::vector<std::string> executedLines;
    try {
      std::string pc = arangodb::basics::FileUtils::slurp(pcPath);
      executedLines = basics::StringUtils::split(pc, '\n');
      if (!executedLines.empty() > 0) {
        executedLines.pop_back();
      }
    } catch (std::exception const&) {
      return;
    }
    if (executedLines.size() >= progLines.size()) {
      return;
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
        arangodb::basics::FileUtils::appendToFile("/tmp/debuggingSLPCurrent",
                                                  current);
      } catch (std::exception const&) {
        // ignore
      }
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
}

void observeGlobalEvent(std::string_view id, std::string_view selector) {
  std::string path = TRI_GetTempPath();
  auto pos = path.rfind(TRI_DIR_SEPARATOR_CHAR);
  if (pos != std::string::npos) {
    path = path.substr(0, pos);
  }
  std::string progPath =
      basics::FileUtils::buildFilename(path.c_str(), "globalSLP");
  std::string pcPath =
      basics::FileUtils::buildFilename(path.c_str(), "globalSLP_PC");

  std::vector<std::string> progLines;
  // Read program:
  try {
    std::string prog = arangodb::basics::FileUtils::slurp(progPath);
    if (prog.empty()) {
      return;
    }
    progLines = arangodb::basics::StringUtils::split(prog, '\n');
    progLines.pop_back();  // Ignore last line, since every line is
                           // ended by \n.
    if (progLines.empty()) {
      return;
    }
  } catch (std::exception const&) {
    return;  // No program, no waiting
  }
  LOG_TOPIC("ace35", INFO, Logger::MAINTENANCE)
      << "Observing global event " << id << " with selector " << selector
      << "...";
  // Read pc
  std::vector<std::string> executedLines;
  try {
    std::string pc = arangodb::basics::FileUtils::slurp(pcPath);
    executedLines = basics::StringUtils::split(pc, '\n');
    if (!executedLines.empty() > 0) {
      executedLines.pop_back();
    }
  } catch (std::exception const&) {
    return;
  }
  if (executedLines.size() >= progLines.size()) {
    return;
  }
  std::string current = progLines[executedLines.size()];
  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(current, ' ');
  if (parts.size() >= 2 && id == parts[0] && selector == parts[1]) {
    // Hurray! We can make progress:
    if (parts.size() >= 3) {
      LOG_TOPIC("ace33", INFO, Logger::MAINTENANCE)
          << "Global event " << id << " with selector " << selector
          << " and comment " << parts[2] << " was observed...";
    } else {
      LOG_TOPIC("ace34", INFO, Logger::MAINTENANCE)
          << "Global event " << id << " with selector " << selector
          << " was observed...";
    }
    current.push_back('\n');
    try {
      arangodb::basics::FileUtils::appendToFile("/tmp/debuggingSLPCurrent",
                                                current);
    } catch (std::exception const&) {
      // ignore
    }
  }
}
}  // namespace arangodb
