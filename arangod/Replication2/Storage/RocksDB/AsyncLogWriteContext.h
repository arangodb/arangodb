////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstdint>

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteContext {
  AsyncLogWriteContext(std::uint64_t vocbaseId, std::uint64_t objectId)
      : vocbaseId(vocbaseId), objectId(objectId) {}
  std::uint64_t const vocbaseId;
  std::uint64_t const objectId;

  void addPendingAsyncOperation();
  void finishPendingAsyncOperation();
  void waitForCompletion();

 private:
// There are currently bugs in the libstdc++ implementation of atomic
// wait/notify, e.g. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106183
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=101037
// . Using the following types should make sure we're not running into them.
#if !defined(_LIBCPP_VERSION) || defined(__linux__) || \
    (defined(_AIX) && !defined(__64BIT__))
  using wait_t = std::int32_t;
#else
  using wait_t = std::int64_t;
#endif
  std::atomic<wait_t> _pendingAsyncOperations{0};
};

}  // namespace arangodb::replication2::storage::rocksdb
