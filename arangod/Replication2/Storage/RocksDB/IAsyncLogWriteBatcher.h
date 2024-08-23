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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/Storage/IStorageEngineMethods.h"

namespace arangodb::futures {
template<typename T>
class Future;
}  // namespace arangodb::futures

namespace arangodb {
template<typename T>
class ResultT;
}

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteContext;

struct IAsyncLogWriteBatcher {
  virtual ~IAsyncLogWriteBatcher() = default;

  struct WriteOptions {
    bool waitForSync = false;
  };

  using SequenceNumber = IStorageEngineMethods::SequenceNumber;

  virtual auto queueInsert(AsyncLogWriteContext& ctx,
                           std::unique_ptr<LogIterator> iter,
                           WriteOptions const& opts)
      -> futures::Future<ResultT<SequenceNumber>> = 0;

  virtual auto queueRemoveFront(AsyncLogWriteContext& ctx, LogIndex stop,
                                WriteOptions const& opts)
      -> futures::Future<ResultT<SequenceNumber>> = 0;

  virtual auto queueRemoveBack(AsyncLogWriteContext& ctx, LogIndex start,
                               WriteOptions const& opts)
      -> futures::Future<ResultT<SequenceNumber>> = 0;
  virtual auto waitForSync(SequenceNumber seq) -> futures::Future<Result> = 0;
};

}  // namespace arangodb::replication2::storage::rocksdb
