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

#include "RocksDBEngine/RocksDBReplicationCommon.h"

using namespace arangodb;

RocksDBReplicationResult::RocksDBReplicationResult(int errorNumber, uint64_t maxTick)
    : _result(errorNumber), _maxTick(maxTick), _lastScannedTick(0), _minTickIncluded(false) {}

RocksDBReplicationResult::RocksDBReplicationResult(int errorNumber, char const* errorMessage,
                                                   uint64_t maxTick)
    : _result(errorNumber, errorMessage),
      _maxTick(maxTick),
      _lastScannedTick(0),
      _minTickIncluded(false) {}

void RocksDBReplicationResult::reset(Result const& other) {
  _result.reset(other);
}

uint64_t RocksDBReplicationResult::maxTick() const { return _maxTick; }

bool RocksDBReplicationResult::minTickIncluded() const {
  return _minTickIncluded;
}

void RocksDBReplicationResult::includeMinTick() { _minTickIncluded = true; }
