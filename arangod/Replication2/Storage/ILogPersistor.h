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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/PersistedLogEntry.h"
#include "Replication2/Storage/IPersistor.h"
#include "Replication2/Storage/IteratorPosition.h"

namespace arangodb {
template<typename T>
class ResultT;
}  // namespace arangodb

namespace arangodb::futures {
struct Unit;
template<typename T>
class Future;
}  // namespace arangodb::futures

namespace arangodb::replication2 {
struct LogIterator;
}
namespace arangodb::replication2::storage {

struct ILogPersistor : virtual IPersistor {
  virtual ~ILogPersistor() = default;

  [[nodiscard]] virtual auto getIterator(IteratorPosition position)
      -> std::unique_ptr<PersistedLogIterator> = 0;

  struct WriteOptions {
    bool waitForSync = false;
  };

  using SequenceNumber = std::uint64_t;

  virtual auto insert(std::unique_ptr<LogIterator>, WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> = 0;
  virtual auto removeFront(LogIndex stop, WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> = 0;
  virtual auto removeBack(LogIndex start, WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> = 0;
  virtual auto getLogId() -> LogId = 0;

  virtual auto waitForSync(SequenceNumber) -> futures::Future<Result> = 0;

  // waits for all ongoing requests to be done
  virtual void waitForCompletion() noexcept = 0;

  virtual auto compact() -> Result = 0;
};

}  // namespace arangodb::replication2::storage
