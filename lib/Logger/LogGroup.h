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

#ifndef ARANGODB_LOGGER_LOG_GROUP_H
#define ARANGODB_LOGGER_LOG_GROUP_H 1

#include <atomic>
#include <cstddef>

namespace arangodb {

class LogGroup {
 public:
  // @brief Number of log groups; must increase this when a new group is added
  // we currently use the following log groups:
  // - 0: default log group: normal logging
  // - 1: audit log group: audit logging
  static constexpr std::size_t Count = 2;

  LogGroup() 
      : _maxLogEntryLength(256U * 1048576U) {}

  virtual ~LogGroup() = default;

  /// @brief Must return a UNIQUE identifier amongst all LogGroup derivatives
  /// and must be less than Count
  virtual std::size_t id() const = 0;

  /// @brief max length of log entries in this group
  std::size_t maxLogEntryLength() const noexcept {
    return _maxLogEntryLength.load(std::memory_order_relaxed);
  }
  
  /// @brief set the max length of log entries in this group.
  /// should not be called during the setup of the Logger, and not at runtime
  void maxLogEntryLength(std::size_t value) {
    _maxLogEntryLength.store(value);
  }

 protected:
  /// @brief maximum length of log entries in this LogGroup
  std::atomic<std::size_t> _maxLogEntryLength;
};

}  // namespace arangodb

#endif
