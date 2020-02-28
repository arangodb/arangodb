////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Builder.h>
#include "gtest/gtest.h"

#include "Aql/AqlCall.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

class DeSerializeAqlCallTest : public ::testing::TestWithParam<AqlCall> {
 public:
  DeSerializeAqlCallTest() = default;

  void SetUp() override {
    aqlCall = GetParam();
  }

 protected:
  AqlCall aqlCall{};
};

auto const testingAqlCalls = ::testing::ValuesIn(std::array{
    AqlCall{0, false, AqlCall::Infinity{}},
    AqlCall{3, false, AqlCall::Infinity{}},
    AqlCall{0, false, 7, AqlCall::LimitType::SOFT},
    AqlCall{0, false, 7, AqlCall::LimitType::HARD},
    AqlCall{0, true, 7, AqlCall::LimitType::HARD},
    AqlCall{3, false, 7, AqlCall::LimitType::SOFT},
    AqlCall{3, false, 7, AqlCall::LimitType::HARD},
    AqlCall{3, true, 7, AqlCall::LimitType::HARD},
});

TEST_P(DeSerializeAqlCallTest, testSuite) {
  auto builder = velocypack::Builder{};
  aqlCall.toVelocyPack(builder);

  ASSERT_TRUE(builder.isClosed());

  auto const maybeDeSerializedCall = std::invoke([&]() {
    try {
      return AqlCall::fromVelocyPack(builder.slice());
    } catch (std::exception const& ex) {
      EXPECT_TRUE(false) << ex.what();
    }
    return ResultT<AqlCall>::error(-1);
  });

  ASSERT_TRUE(maybeDeSerializedCall.ok()) << maybeDeSerializedCall.errorMessage();

  auto const deSerializedCall = *maybeDeSerializedCall;

  ASSERT_EQ(aqlCall, deSerializedCall);
}

INSTANTIATE_TEST_CASE_P(DeSerializeAqlCallTestVariations, DeSerializeAqlCallTest, testingAqlCalls);

}  // namespace arangodb::tests::aql
