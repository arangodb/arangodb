////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TESTS_AQL_ITEM_BLOCK_HELPER_H
#define ARANGOD_AQL_TESTS_AQL_ITEM_BLOCK_HELPER_H

#include <memory>
#include <array>

#include <boost/variant.hpp>

#include "Aql/AqlItemBlock.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include "AqlHelper.h"
#include "VelocyPackHelper.h"

/* * * * * * * *
 * * SYNOPSIS  *
 * * * * * * * *

build a matrix with 4 rows and 3 columns
the number of columns has to be specified as a template parameter:

  SharedAqlItemBlockPtr block = buildBlock<3>({
    { {1}, {2}, {R"({ "iam": [ "a", "json" ] })"} },
    { {4}, {5}, {"\"and will be converted\""} },
    { {7}, {8}, {R"({ "into": [], "a": [], "vpack": [] })"} },
    { {10}, {11}, {12} },
  });

Currently supported types for values are `int` and `const char*`.

Also, print the block like this:

  std::cout << *block;

1, 2, {"iam":["a","json"]}
4, 5, "and will be converted"
7, 8, {"a":[],"into":[],"vpack":[]}
10, 11, 12

 */

namespace arangodb {
namespace tests {
namespace aql {

using EntryBuilder = boost::variant<int, const char*>;

template <::arangodb::aql::RegisterId columns>
using RowBuilder = std::array<EntryBuilder, columns>;

template <::arangodb::aql::RegisterId columns>
using MatrixBuilder = std::vector<RowBuilder<columns>>;

template <::arangodb::aql::RegisterId columns>
::arangodb::aql::SharedAqlItemBlockPtr buildBlock(::arangodb::aql::AqlItemBlockManager& manager,
                                                  MatrixBuilder<columns>&& matrix);

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

namespace arangodb {
namespace tests {
namespace aql {

using namespace ::arangodb::aql;

class EntryToAqlValueVisitor : public boost::static_visitor<AqlValue> {
 public:
  AqlValue operator()(int i) const { return AqlValue{AqlValueHintInt{i}}; }

  AqlValue operator()(const char* json) const {
    VPackBufferPtr tmpVpack = vpackFromJsonString(json);
    return AqlValue{AqlValueHintCopy{tmpVpack->data()}};
  }
};

template <RegisterId columns>
SharedAqlItemBlockPtr buildBlock(AqlItemBlockManager& manager,
                                 MatrixBuilder<columns>&& matrix) {
  if (matrix.size() == 0) {
    return nullptr;
  }
  SharedAqlItemBlockPtr block{new AqlItemBlock(manager, matrix.size(), columns)};

  for (size_t row = 0; row < matrix.size(); row++) {
    for (RegisterId col = 0; col < columns; col++) {
      auto const& entry = matrix[row][col];
      auto visitor = EntryToAqlValueVisitor();
      block->setValue(row, col, boost::apply_visitor(visitor, entry));
    }
  }

  return block;
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

#endif  // ARANGOD_AQL_TESTS_AQL_ITEM_BLOCK_HELPER_H
