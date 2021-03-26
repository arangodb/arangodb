////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGO_ROCKSDB_ROCKSDB_BACKGROUND_ERROR_LISTENER_H
#define ARANGO_ROCKSDB_ROCKSDB_BACKGROUND_ERROR_LISTENER_H 1

// public rocksdb headers
#include <rocksdb/db.h>
#include <rocksdb/listener.h>

#include <atomic>

namespace arangodb {

class RocksDBBackgroundErrorListener : public rocksdb::EventListener {
 public:
  RocksDBBackgroundErrorListener();
  ~RocksDBBackgroundErrorListener();

  void OnBackgroundError(rocksdb::BackgroundErrorReason reason, rocksdb::Status* error) override;

  bool called() const noexcept;
  void resume();

 private:
  std::atomic<bool> _called;
};  // class RocksDBThrottle

}  // namespace arangodb

#endif
