////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_REPLICATION_COMMON_H
#define ARANGO_ROCKSDB_ROCKSDB_REPLICATION_COMMON_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"

namespace arangodb {

class RocksDBReplicationResult : public Result {
 public:
  RocksDBReplicationResult(int errorNumber, uint64_t lastTick);
  RocksDBReplicationResult(int errorNumber, char const* errorMessage, uint64_t lastTick);
  uint64_t maxTick() const;
  uint64_t lastScannedTick() const { return _lastScannedTick; }
  void lastScannedTick(uint64_t lastScannedTick) {
    _lastScannedTick = lastScannedTick;
  }
  bool minTickIncluded() const;
  void includeMinTick();

 private:
  uint64_t _maxTick;
  uint64_t _lastScannedTick;
  bool _minTickIncluded;
};

}  // namespace arangodb

#endif
