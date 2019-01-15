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

#ifndef ARANGOD_AQL_RANGE_H
#define ARANGOD_AQL_RANGE_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace aql {

/// @brief Range, to hold a range compactly
struct Range {
  Range() = delete;

  Range(int64_t low, int64_t high);

  size_t size() const;

  int64_t at(size_t position) const;

  bool isIn(int64_t value) const;

  int64_t const _low;
  int64_t const _high;
};
}  // namespace aql
}  // namespace arangodb

#endif
