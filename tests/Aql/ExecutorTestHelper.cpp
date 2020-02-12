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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ExecutorTestHelper.h"

#include "Aql/ExecutionEngine.h"

using namespace arangodb::tests::aql;

auto arangodb::tests::aql::ValidateBlocksAreEqual(SharedAqlItemBlockPtr actual,
                                                  SharedAqlItemBlockPtr expected) -> void {
  velocypack::Options vpackOptions;
  ASSERT_NE(expected, nullptr);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(actual->size(), expected->size());
  EXPECT_EQ(actual->getNrRegs(), 1);
  for (size_t i = 0; i < (std::min)(actual->size(), expected->size()); ++i) {
    auto const& x = actual->getValueReference(i, 0);
    auto const& y = expected->getValueReference(i, 0);
    EXPECT_TRUE(AqlValue::Compare(&vpackOptions, x, y, true) == 0)
        << "Row " << i << " Column " << 0 << " do not agree. "
        << x.slice().toJson(&vpackOptions) << " vs. "
        << y.slice().toJson(&vpackOptions);
  }
}

template <bool enableQueryTrace>
AqlExecutorTestCase<enableQueryTrace>::AqlExecutorTestCase()
    : _server{}, fakedQuery{_server.createFakeQuery(enableQueryTrace)} {
  auto engine = std::make_unique<ExecutionEngine>(*fakedQuery, SerializationFormat::SHADOWROWS);
  fakedQuery->setEngine(engine.release());
}

template <bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::generateNodeDummy() -> ExecutionNode* {
  auto dummy = std::make_unique<SingletonNode>(fakedQuery->plan(), _execNodes.size());
  auto res = dummy.get();
  _execNodes.emplace_back(std::move(dummy));
  return res;
}
template <bool enableQueryTrace>
auto AqlExecutorTestCase<enableQueryTrace>::manager() const -> AqlItemBlockManager& {
  return fakedQuery->engine()->itemBlockManager();
}

template class ::arangodb::tests::aql::AqlExecutorTestCase<true>;
template class ::arangodb::tests::aql::AqlExecutorTestCase<false>;

std::ostream& arangodb::tests::aql::operator<<(std::ostream& stream,
                                               arangodb::tests::aql::ExecutorCall call) {
  return stream << [call]() {
    switch (call) {
      case ExecutorCall::SKIP_ROWS:
        return "SKIP_ROWS";
      case ExecutorCall::PRODUCE_ROWS:
        return "PRODUCE_ROWS";
      case ExecutorCall::FETCH_FOR_PASSTHROUGH:
        return "FETCH_FOR_PASSTHROUGH";
      case ExecutorCall::EXPECTED_NR_ROWS:
        return "EXPECTED_NR_ROWS";
    }
    // The control flow cannot reach this. It is only here to make MSVC happy,
    // which is unable to figure out that the switch above is complete.
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
  }();
}
