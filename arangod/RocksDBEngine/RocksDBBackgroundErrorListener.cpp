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

#include "RocksDBBackgroundErrorListener.h"

#include "Logger/Logger.h"

namespace arangodb {

RocksDBBackgroundErrorListener::~RocksDBBackgroundErrorListener() {}

void RocksDBBackgroundErrorListener::OnBackgroundError(rocksdb::BackgroundErrorReason reason,
                                                       rocksdb::Status*) {
  if (!_called) {
    _called = true;

    std::string operation = "unknown";
    switch (reason) {
      case rocksdb::BackgroundErrorReason::kFlush: {
        operation = "flush";
        break;
      }
      case rocksdb::BackgroundErrorReason::kCompaction: {
        operation = "compaction";
        break;
      }
      case rocksdb::BackgroundErrorReason::kWriteCallback: {
        operation = "write callback";
        break;
      }
      case rocksdb::BackgroundErrorReason::kMemTable: {
        operation = "memtable";
        break;
      }
    }

    LOG_TOPIC(ERR, Logger::ROCKSDB)
        << "RocksDB encountered a background error during a " << operation
        << " operation; The database will be put in read-only "
           "mode, and subsequent write errors are likely";
  }
}

}  // namespace arangodb
