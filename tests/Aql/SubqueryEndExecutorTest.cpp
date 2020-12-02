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
/// @author Michael Hackstein
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Mocks/Death_Test.h"

#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SubqueryEndExecutor.h"

#include "Logger/LogMacros.h"

#include "Basics/VelocyPackHelper.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::basics;

using RegisterSet = std::unordered_set<RegisterId>;

class SubqueryEndExecutorTest : public ::testing::Test {
 public:
  SubqueryEndExecutorTest()
      : _infos(nullptr, monitor, RegisterId{0}, RegisterId{0}) {}

 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{monitor, SerializationFormat::SHADOWROWS};
  SubqueryEndExecutorInfos _infos;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher{
      itemBlockManager, VPackParser::fromJson("[]")->steal(), false};

  void ExpectedValues(OutputAqlItemRow& itemRow,
                      std::vector<std::vector<std::string>> const& expectedStrings,
                      std::unordered_map<size_t, uint64_t> const& shadowRows) const {
    auto block = itemRow.stealBlock();

    ASSERT_EQ(expectedStrings.size(), block->size());

    for (size_t rowIdx = 0; rowIdx < block->size(); rowIdx++) {
      if (block->isShadowRow(rowIdx)) {
        ShadowAqlItemRow shadow{block, rowIdx};

        auto depth = shadowRows.find(rowIdx);
        if (depth != shadowRows.end()) {
          EXPECT_EQ(depth->second, shadow.getDepth());
        } else {
          FAIL() << "did not expect row " << rowIdx << " to be a shadow row";
        }
      } else {
        EXPECT_EQ(shadowRows.find(rowIdx), shadowRows.end())
            << "expected row " << rowIdx << " to be a shadow row";

        InputAqlItemRow input{block, rowIdx};
        for (unsigned int colIdx = 0; colIdx < block->getNrRegs(); colIdx++) {
          auto expected = VPackParser::fromJson(expectedStrings.at(rowIdx).at(colIdx));
          auto value = input.getValue(RegisterId{colIdx}).slice();
          EXPECT_TRUE(VelocyPackHelper::equal(value, expected->slice(), false))
              << value.toJson() << " != " << expected->toJson();
        }
      }
    }
  }
};

TEST_F(SubqueryEndExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryEndExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds additional "
         "rows only.";
  EXPECT_EQ(SubqueryEndExecutor::Properties::allowsBlockPassthrough, ::arangodb::aql::BlockPassthrough::Disable)
      << "The block cannot be passThrough, as it increases the number of rows.";
  EXPECT_TRUE(SubqueryEndExecutor::Properties::inputSizeRestrictsOutputSize)
      << "The block produces one output row per input row plus potentially a "
         "shadow rows which is bounded by the structure of the query";
};
