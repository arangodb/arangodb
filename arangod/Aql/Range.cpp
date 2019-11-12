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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Range.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

using namespace arangodb::aql;

/// @brief Range constructor
Range::Range(int64_t low, int64_t high) : _low(low), _high(high) {}

/// @brief get number of elements in Range
size_t Range::size() const {
  if (_low <= _high) {
    // e.g. 1..1, 1..10 etc.
    return static_cast<size_t>(_high - _low + 1);
  }
  // e.g. 10..1
  return static_cast<size_t>(_low - _high + 1);
}

/// @brief get element at position
int64_t Range::at(size_t position) const {
  if (_low <= _high) {
    // e.g. 1..1, 1..10 etc.
    return _low + static_cast<int64_t>(position);
  }

  // e.g. 10..1
  return _low - static_cast<int64_t>(position);
}

bool Range::isIn(int64_t value) const {
  if (_low <= _high) {
    // e.g. 1..1, 1..10 etc.
    return value >= _low && value <= _high;
  }
  // e.g. 10..1
  return value <= _low && value >= _high;
}
  
void Range::throwIfTooBigForMaterialization(uint64_t values) {
  if (values > MaterializationLimit) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, "range is too big to be materialized");
  }
}
