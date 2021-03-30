////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGODB3_PERSISTEDLOG_H
#define ARANGODB3_PERSISTEDLOG_H
#include <memory>
#include "Basics/Result.h"
#include "Common.h"

namespace arangodb::replication2 {

struct PersistedLog {
  virtual ~PersistedLog() = default;
  explicit PersistedLog(LogId lid) : _lid(lid) {}

  [[nodiscard]] auto id() const noexcept -> LogId { return _lid; }
  virtual auto insert(std::shared_ptr<LogIterator> iter) -> Result = 0;
  virtual auto read(LogIndex start) -> std::shared_ptr<LogIterator> = 0;
  virtual auto removeFront(LogIndex stop) -> Result = 0;
  virtual auto removeBack(LogIndex start) -> Result = 0;

  virtual auto drop() -> Result = 0;
 private:
  LogId _lid;
};

}

#endif  // ARANGODB3_PERSISTEDLOG_H
