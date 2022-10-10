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
#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"

using namespace arangodb::replication2;

namespace arangodb::replication2::test {

struct FakeStorageEngineMethods : replicated_state::IStorageEngineMethods {
  auto updateMetadata(replicated_state::PersistedStateInfo info)
      -> Result override {
    meta = std::move(info);
    return {};
  }

  auto readMetadata()
      -> ResultT<replicated_state::PersistedStateInfo> override {
    if (meta.has_value()) {
      return {*meta};
    } else {
      return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND};
    }
  }

  auto read(LogIndex first) -> std::unique_ptr<PersistedLogIterator> override {
    TRI_ASSERT(false) << "not implemented";
  }

  auto insert(std::unique_ptr<PersistedLogIterator> ptr,
              const WriteOptions& options)
      -> futures::Future<ResultT<SequenceNumber>> override {
    TRI_ASSERT(false) << "not implemented";
  }

  auto removeFront(LogIndex stop, const WriteOptions& options)
      -> futures::Future<ResultT<SequenceNumber>> override {
    TRI_ASSERT(false) << "not implemented";
  }

  auto removeBack(LogIndex start, const WriteOptions& options)
      -> futures::Future<ResultT<SequenceNumber>> override {
    TRI_ASSERT(false) << "not implemented";
  }

  auto getObjectId() -> std::uint64_t override { return objectId; }
  auto getLogId() -> LogId override { return logId; }
  auto getSyncedSequenceNumber() -> SequenceNumber override {
    TRI_ASSERT(false) << "not implemented";
  }
  auto waitForSync(SequenceNumber number)
      -> futures::Future<futures::Unit> override {
    TRI_ASSERT(false) << "not implemented";
  }

  std::uint64_t objectId;
  LogId logId;
  std::optional<replicated_state::PersistedStateInfo> meta;
  std::deque<PersistingLogEntry> log;
  SequenceNumber lastSequenceNumber;
};
}  // namespace arangodb::replication2::test

struct ReplicatedLogTest2 : ::testing::Test {};

TEST_F(ReplicatedLogTest2, foo_bar) {
  // auto log = std::make_shared<replicated_log::ReplicatedLog>();
}
