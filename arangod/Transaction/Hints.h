////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
    LOCK_ENTIRELY = 2,
    LOCK_NEVER = 4,
    NO_BEGIN_MARKER = 8,
    NO_ABORT_MARKER = 16,
    NO_THROTTLING = 32,
    TRY_LOCK = 64,
    NO_COMPACTION_LOCK = 128,
    NO_USAGE_LOCK = 256,
    RECOVERY = 512,
    NO_DLD = 1024, // disable deadlock detection
    READ_OWN_WRITES = 2048 // do not use snapshot
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
  
  inline void set(ValueType value) {
    _value |= value;
  }
  
  inline void set(Hint value) {
    set(static_cast<ValueType>(value));
  }
  
  inline void unset(ValueType value) {
    _value &= ~value;
  }
  
  inline void unset(Hint value) {
    unset(static_cast<ValueType>(value));
  }
  
  inline ValueType toInt() const {
    return static_cast<ValueType>(_value);
  }

 private:
  ValueType _value;
};

}
}

#endif
