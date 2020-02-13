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

auto asserthelper::AqlValuesAreIdentical(AqlValue const& lhs, AqlValue const& rhs) -> bool {
  velocypack::Options vpackOptions;
  return AqlValue::Compare(&vpackOptions, lhs, rhs, true) == 0;
}

auto asserthelper::RowsAreIdentical(SharedAqlItemBlockPtr actual, size_t actualRow,
                                    SharedAqlItemBlockPtr expected, size_t expectedRow,
                                    std::optional<std::vector<RegisterId>> const& onlyCompareRegisters)
    -> bool {
  if (onlyCompareRegisters) {
    if (actual->getNrRegs() >= onlyCompareRegisters->size()) {
      // Registers do not match
      return false;
    }
  } else {
    if (actual->getNrRegs() != expected->getNrRegs()) {
      // Registers do not match
      return false;
    }
  }

  for (RegisterId reg = 0; reg < expected->getNrRegs(); ++reg) {
    auto const& x =
        actual->getValueReference(actualRow, onlyCompareRegisters
                                                 ? onlyCompareRegisters->at(reg)
                                                 : reg);
    auto const& y = expected->getValueReference(expectedRow, reg);
    if (!AqlValuesAreIdentical(x, y)) {
      // At least one value mismatched
      return false;
    }
  }
  // All values match
  return true;
}

auto asserthelper::ValidateAqlValuesAreEqual(SharedAqlItemBlockPtr actual,
                                             size_t actualRow, RegisterId actualRegister,
                                             SharedAqlItemBlockPtr expected, size_t expectedRow,
                                             RegisterId expectedRegister) -> void {
  velocypack::Options vpackOptions;
  auto const& x = actual->getValueReference(actualRow, actualRegister);
  auto const& y = expected->getValueReference(expectedRow, expectedRegister);
  EXPECT_TRUE(AqlValuesAreIdentical(x, y))
      << "Row " << actualRow << " Column " << actualRegister << " do not agree. "
      << x.slice().toJson(&vpackOptions) << " vs. " << y.slice().toJson(&vpackOptions);
}

auto asserthelper::ValidateBlocksAreEqual(SharedAqlItemBlockPtr actual,
                                          SharedAqlItemBlockPtr expected,
                                          std::optional<std::vector<RegisterId>> const& onlyCompareRegisters)
    -> void {
  ASSERT_NE(expected, nullptr);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(actual->size(), expected->size());
  RegisterId outRegs = (std::min)(actual->getNrRegs(), expected->getNrRegs());
  if (onlyCompareRegisters) {
    outRegs = onlyCompareRegisters->size();
    ASSERT_GE(actual->getNrRegs(), outRegs);
  } else {
    EXPECT_EQ(actual->getNrRegs(), expected->getNrRegs());
  }

  for (size_t row = 0; row < (std::min)(actual->size(), expected->size()); ++row) {
    for (RegisterId reg = 0; reg < outRegs; ++reg) {
      RegisterId actualRegister =
          onlyCompareRegisters ? onlyCompareRegisters->at(reg) : reg;
      ValidateAqlValuesAreEqual(actual, row, actualRegister, expected, row, reg);
    }
  }
}

auto asserthelper::ValidateBlocksAreEqualUnordered(
    SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
    std::optional<std::vector<RegisterId>> const& onlyCompareRegisters) -> void {
  ASSERT_NE(expected, nullptr);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(actual->size(), expected->size());
  EXPECT_EQ(actual->getNrRegs(), expected->getNrRegs());
  for (size_t expectedRow = 0; expectedRow < expected->size(); ++expectedRow) {
    bool found = false;
    for (size_t actualRow = 0; actualRow < actual->size(); ++actualRow) {
      if (RowsAreIdentical(actual, actualRow, expected, expectedRow, onlyCompareRegisters)) {
        found = true;
        // one is enough
        break;
      }
    }
    EXPECT_TRUE(found) << "Did not find expected row nr " << expectedRow;
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
