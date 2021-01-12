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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_TRANSACTION_HINTS_H
#define ARANGOD_TRANSACTION_HINTS_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace transaction {

class Hints {
 public:
  typedef uint32_t ValueType;

  /// @brief individual hint flags that can be used for transactions
  enum class Hint : ValueType {
    NONE = 0,
    SINGLE_OPERATION = 1,
    LOCK_NEVER = 2,
    NO_USAGE_LOCK = 4,
    NO_DLD = 8,        // disable deadlock detection, unsupported in RocksDB
    NO_INDEXING = 16,  // use DisableIndexing for RocksDB
    INTERMEDIATE_COMMITS = 32,  // enable intermediate commits in rocksdb
    ALLOW_RANGE_DELETE = 64,    // enable range-delete in rocksdb
    FROM_TOPLEVEL_AQL = 128,    // transaction is only runnning one AQL query
    GLOBAL_MANAGED = 256,       // transaction with externally managed lifetime
    INDEX_CREATION = 512,  // transaction is for creating index on existing collection (many inserts, no removes, index will be deleted on any failure anyway)
    IS_FOLLOWER_TRX = 1024,  // transaction used to replicate something on a follower
    ALLOW_FAST_LOCK_ROUND_CLUSTER =
        2048,  // allow the coordinator to try a fast-lock path (parallel on all DBServers), and if that fails revert to slow-lock path
  };

  Hints() : _value(0) {}
  explicit Hints(Hint value) : _value(static_cast<ValueType>(value)) {}
  explicit Hints(ValueType value) : _value(value) {}

  inline bool has(ValueType value) const noexcept {
    return (_value & value) != 0;
  }

  inline bool has(Hint value) const noexcept {
    return has(static_cast<ValueType>(value));
  }

  inline void set(ValueType value) { _value |= value; }

  inline void set(Hint value) { set(static_cast<ValueType>(value)); }

  inline void unset(ValueType value) { _value &= ~value; }

  inline void unset(Hint value) { unset(static_cast<ValueType>(value)); }

  inline ValueType toInt() const { return static_cast<ValueType>(_value); }

 private:
  ValueType _value;
};

}  // namespace transaction
}  // namespace arangodb

#endif
