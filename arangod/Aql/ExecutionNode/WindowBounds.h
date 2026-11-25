////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/AqlValue.h"
#include "Aql/QueryWarnings.h"
#include "Basics/datetime.h"
#include "Basics/voc-errors.h"

namespace arangodb::aql {

/// utility class to calculate window bounds for Row / Range based windows
/// could probably also be part of the executor
class WindowBounds final {
 public:
  enum class Type { Row, Range };
  /// range based WINDOW row values
  struct Row {
    double value;
    double lowBound;
    double highBound;
    bool valid;
  };

  WindowBounds(Type type, AqlValue&& preceding, AqlValue&& following);
  WindowBounds(Type type, velocypack::Slice slice);
  ~WindowBounds();

 public:
  int64_t numPrecedingRows() const;
  int64_t numFollowingRows() const;

  bool needsFollowingRows() const;

  Row calcRow(AqlValue const& input, QueryWarnings& q) const;

 public:
  bool unboundedPreceding() const;
  void toVelocyPack(velocypack::Builder& options) const;

 private:
  enum class RangeType { Numeric, Date };
  const Type _type;

  int64_t _numPrecedingRows = 0;
  int64_t _numFollowingRows = 0;
  RangeType _rangeType = RangeType::Numeric;

  // contains calendar aware year + month
  basics::ParsedDuration _precedingDuration;
  basics::ParsedDuration _followingDuration;

  double _precedingNumber = 0.0;
  double _followingNumber = 0.0;
};
}  // namespace arangodb::aql
