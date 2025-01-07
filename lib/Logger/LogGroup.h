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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstddef>

#include "Assertions/Assert.h"

namespace arangodb {

class LogGroup {
 public:
  // @brief Number of log groups; must increase this when a new group is added
  // we currently use the following log groups:
  // - 0: default log group: normal logging
  // - 1: audit log group: audit logging
  static constexpr std::size_t Count = 2;

  LogGroup(std::size_t id) : _maxLogEntryLength(256U * 1048576U), _id(id) {
    TRI_ASSERT(id < Count);
  }

  /// @brief Must return a UNIQUE identifier amongst all LogGroup derivatives
  /// and must be less than Count
  std::size_t id() const noexcept { return _id; }

  /// @brief max length of log entries in this group
  std::size_t maxLogEntryLength() const noexcept {
    return _maxLogEntryLength.load(std::memory_order_relaxed);
  }

  /// @brief set the max length of log entries in this group.
  /// should not be called during the setup of the Logger, and not at runtime
  void maxLogEntryLength(std::size_t value) noexcept {
    _maxLogEntryLength.store(value);
  }

 protected:
  /// @brief maximum length of log entries in this LogGroup
  std::atomic<std::size_t> _maxLogEntryLength;
  std::size_t const _id;
};

}  // namespace arangodb
