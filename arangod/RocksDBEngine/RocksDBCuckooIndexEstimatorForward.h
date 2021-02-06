////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel Larkin
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_INDEX_ESTIMATOR_FORWARD_H
#define ARANGOD_ROCKSDB_ROCKSDB_INDEX_ESTIMATOR_FORWARD_H 1

#include <cstddef>
#include <utility>

namespace arangodb {

// C++ wrapper for the hash function:
template <class T, uint64_t Seed>
class HashWithSeed;

template <class Key, class HashKey = HashWithSeed<Key, 0xdeadbeefdeadbeefULL>,
          class Fingerprint = HashWithSeed<Key, 0xabcdefabcdef1234ULL>,
          class HashShort = HashWithSeed<uint16_t, 0xfedcbafedcba4321ULL>, class CompKey = std::equal_to<Key>>
class RocksDBCuckooIndexEstimator;

}

#endif
