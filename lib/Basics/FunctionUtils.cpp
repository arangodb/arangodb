////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "FunctionUtils.h"

namespace arangodb {
namespace basics {
namespace function_utils {

/**
 * @brief Execute a lambda, retrying periodically until it succeeds or times out
 * @param fn            Lambda to run
 * @param topic         Log topic for any messages
 * @param message       Description for log messages (format below)
 * @param retryInterval Period to wait between attempts
 * @param timeout       Total time to wait before timing out and returning
 *
 * If a given attempt fails, a log message will be made in the following form:
 * "Failed to " + message + ", waiting to retry..."
 */
bool retryUntilTimeout(std::function<bool()> fn, LogTopic& topic,
                       std::string const& message, std::chrono::nanoseconds retryInterval,
                       std::chrono::nanoseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  bool success = false;
  while ((std::chrono::steady_clock::now() - start) < timeout) {
    success = fn();
    if (success) {
      break;
    }
    LOG_TOPIC("18d0b", INFO, topic) << "Failed to " << message << ", waiting to retry...";
    std::this_thread::sleep_for(retryInterval);
  }
  return success;
}

}  // namespace function_utils
}  // namespace basics
}  // namespace arangodb
