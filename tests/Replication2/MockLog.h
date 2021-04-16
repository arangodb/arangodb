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
#ifndef ARANGODB3_MOCKLOG_H
#define ARANGODB3_MOCKLOG_H

#include <Replication2/PersistedLog.h>

#include <map>

namespace arangodb {

struct MockLog : replication2::PersistedLog {

  using storeType = std::map<replication2::LogIndex, replication2::LogEntry>;

  explicit MockLog(replication2::LogId id);
  MockLog(replication2::LogId id, storeType storage);

  ~MockLog() override = default;

  auto insert(std::shared_ptr<replication2::LogIterator> iter) -> Result override;
  auto read(replication2::LogIndex start) -> std::shared_ptr<replication2::LogIterator> override;
  auto removeFront(replication2::LogIndex stop) -> Result override;
  auto removeBack(replication2::LogIndex start) -> Result override;
  auto drop() -> Result override;

  [[nodiscard]] storeType getStorage() const { return _storage; }
 private:
  using iteratorType = storeType::iterator;
  storeType _storage;
};

}  // namespace arangodb

#endif  // ARANGODB3_MOCKLOG_H
