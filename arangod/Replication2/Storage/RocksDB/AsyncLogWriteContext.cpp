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

#include "AsyncLogWriteContext.h"

namespace arangodb::replication2::storage::rocksdb {

void AsyncLogWriteContext::waitForCompletion() {
  while (true) {
    auto c = _pendingAsyncOperations.load(std::memory_order_acquire);
    if (c == 0) {
      break;
    }
    _pendingAsyncOperations.wait(c, std::memory_order_acquire);
  }
}

void AsyncLogWriteContext::finishPendingAsyncOperation() {
  auto c = _pendingAsyncOperations.fetch_sub(1, std::memory_order_release);
  if (c == 1) {
    _pendingAsyncOperations.notify_all();
  }
}

void AsyncLogWriteContext::addPendingAsyncOperation() {
  _pendingAsyncOperations.fetch_add(1, std::memory_order_release);
}

}  // namespace arangodb::replication2::storage::rocksdb
