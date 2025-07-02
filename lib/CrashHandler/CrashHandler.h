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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <string_view>

namespace arangodb {

/// @brief States for the crash handler thread coordination.
/// The state of the dedicated crash handler starts with IDLE
/// and moves to CRASH_DETECTED when a crash is detected. When the
/// thread has handled the situation it goes to HANDLING_COMPLETE
/// to indicate that the crashed thread can continue its crash.
/// The only other state transition possible is on regular shutdown
/// from IDLE to SHUTDOWN to let the thread exit gracefully.
enum class CrashHandlerState : int {
  IDLE = 0,               ///< idle state, waiting for crashes
  CRASH_DETECTED = 1,     ///< crash detected, handling in progress
  HANDLING_COMPLETE = 2,  ///< crash handling complete
  SHUTDOWN = 3            ///< shutdown requested
};

class CrashHandler {
  std::atomic<bool> _threadRunning{false};
  static std::atomic<CrashHandler*> _theCrashHandler;
  // This needs to be static for the signal handlers to reach!

 public:
  CrashHandler() {
    // starts global background thread if not already done
    bool threadRunning = _threadRunning.exchange(true);
    if (!threadRunning) {
      _theCrashHandler.store(this);
      installCrashHandler();
    }
  }

  ~CrashHandler() {
    // joins global background thread if running
    bool threadRunning = _threadRunning.exchange(false);
    if (threadRunning) {
      shutdownCrashHandler();
    }
    _theCrashHandler.store(nullptr);
  }

  /// @brief log backtrace for current thread to logfile
  static void logBacktrace();

  /// @brief set server state string as context for crash messages.
  /// state string will be advanced whenever the application server
  /// changes its state. state string must be null-terminated!
  static void setState(std::string_view state);

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

  /// @brief triggers the crash handler thread to handle a crash
  /// @return true if successfully triggered, false if already in progress
  /// This is used in the signal handlers and in the std::terminate handler
  /// for assertion failures.
  static void triggerCrashHandler();

  /// @brief waits for the crash handler thread to complete its work
  /// This is used in the signal handlers and in the std::terminate handler
  /// for assertion failures.
  static void waitForCrashHandlerCompletion();

 private:
  /// @brief installs the crash handler globally
  static void installCrashHandler();

  /// @brief shuts down the crash handler thread
  static void shutdownCrashHandler();
};

}  // namespace arangodb
