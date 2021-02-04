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

#ifndef ARANGODB_BASICS_FUNCTION_UTILS_H
#define ARANGODB_BASICS_FUNCTION_UTILS_H 1

#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>
#include <tuple>

#include "Logger/LogMacros.h"

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
template <typename R>
std::pair<bool, R> retryUntilTimeout(
    std::function<std::pair<bool, R>()> fn, LogTopic& topic, std::string const& message,
    std::chrono::nanoseconds retryInterval = std::chrono::seconds(1),
    std::chrono::nanoseconds timeout = std::chrono::minutes(5)) {
  auto start = std::chrono::steady_clock::now();
  bool success = false;
  R value{};
  while ((std::chrono::steady_clock::now() - start) < timeout) {
    std::tie(success, value) = fn();
    if (success) {
      break;
    }
    LOG_TOPIC("18d0a", INFO, topic) << "Failed to " << message << ", waiting to retry...";
    std::this_thread::sleep_for(retryInterval);
  }
  return std::make_pair(success, value);
}

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
bool retryUntilTimeout(std::function<bool()> fn, LogTopic& topic, std::string const& message,
                       std::chrono::nanoseconds retryInterval = std::chrono::seconds(1),
                       std::chrono::nanoseconds timeout = std::chrono::minutes(5));

}  // namespace function_utils
}  // namespace basics
}  // namespace arangodb

#endif
