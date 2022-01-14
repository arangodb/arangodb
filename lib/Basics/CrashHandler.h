////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

namespace arangodb {
class CrashHandler {
 public:
  /// @brief log backtrace for current thread to logfile
  static void logBacktrace();

  /// @brief logs a fatal message and crashes the program
  [[noreturn]] static void crash(std::string_view context);

  /// @brief logs an assertion failure and crashes the program
  [[noreturn]] static void assertionFailure(char const* file, int line,
                                            char const* func,
                                            char const* context,
                                            const char* message);

  /// @brief set flag to kill process hard using SIGKILL, in order to circumvent
  /// core file generation etc.
  static void setHardKill();

  /// @brief disable printing of backtraces
  static void disableBacktraces();

  /// @brief installs the crash handler globally
  static void installCrashHandler();

#ifdef _WIN32
  static void setMiniDumpDirectory(std::string path);
#endif
};

}  // namespace arangodb
