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

#include "Aql/AqlItemBlockManager.h"
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
    if (actual->getNrRegs() < onlyCompareRegisters->size()) {
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
  auto outRegs = (std::min)(actual->getNrRegs(), expected->getNrRegs());
  if (onlyCompareRegisters) {
    outRegs = onlyCompareRegisters->size();
    ASSERT_GE(actual->getNrRegs(), outRegs);
  } else {
    EXPECT_EQ(actual->getNrRegs(), expected->getNrRegs());
  }

  for (size_t row = 0; row < (std::min)(actual->size(), expected->size()); ++row) {
    // Compare registers
    for (RegisterId reg = 0; reg < outRegs; ++reg) {
      RegisterId actualRegister =
          onlyCompareRegisters ? onlyCompareRegisters->at(reg) : reg;
      ValidateAqlValuesAreEqual(actual, row, actualRegister, expected, row, reg);
    }
    // Compare shadowRows
    EXPECT_EQ(actual->isShadowRow(row), expected->isShadowRow(row));
    if (actual->isShadowRow(row) && expected->isShadowRow(row)) {
      ShadowAqlItemRow actualShadowRow{actual, row};
      ShadowAqlItemRow expectedShadowRow{expected, row};
      EXPECT_EQ(actualShadowRow.getDepth(), expectedShadowRow.getDepth());
    }
  }
}

auto asserthelper::ValidateBlocksAreEqualUnordered(
    SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected, std::size_t numRowsNotContained,
    std::optional<std::vector<RegisterId>> const& onlyCompareRegisters) -> void {
  std::unordered_set<size_t> matchedRows{};
  return ValidateBlocksAreEqualUnordered(actual, expected, matchedRows,
                                         numRowsNotContained, onlyCompareRegisters);
}

auto asserthelper::ValidateBlocksAreEqualUnordered(
    SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
    std::unordered_set<size_t>& matchedRows, std::size_t numRowsNotContained,
    std::optional<std::vector<RegisterId>> const& onlyCompareRegisters) -> void {
  ASSERT_NE(expected, nullptr);
  ASSERT_NE(actual, nullptr);
  EXPECT_FALSE(actual->hasShadowRows())
      << "unordered validation does not support shadowRows yet. If you need "
         "this please implement!";
  EXPECT_FALSE(expected->hasShadowRows())
      << "unordered validation does not support shadowRows yet. If you need "
         "this please implement!";

  EXPECT_EQ(actual->size() + numRowsNotContained, expected->size());

  auto outRegs = (std::min)(actual->getNrRegs(), expected->getNrRegs());
  if (onlyCompareRegisters) {
    outRegs = onlyCompareRegisters->size();
    ASSERT_GE(actual->getNrRegs(), outRegs);
  } else {
    EXPECT_EQ(actual->getNrRegs(), expected->getNrRegs());
  }

  matchedRows.clear();

  for (size_t expectedRow = 0; expectedRow < expected->size(); ++expectedRow) {
    for (size_t actualRow = 0; actualRow < actual->size(); ++actualRow) {
      if (RowsAreIdentical(actual, actualRow, expected, expectedRow, onlyCompareRegisters)) {
        auto const& [unused, inserted] = matchedRows.emplace(expectedRow);
        if (inserted) {
          // one is enough, but do not match the same rows twice
          break;
        }
      }
    }
  }

  if (matchedRows.size() + numRowsNotContained < expected->size()) {
    // Did not find all rows.
    // This is for reporting only:
    for (size_t expectedRow = 0; expectedRow < expected->size(); ++expectedRow) {
      if (matchedRows.find(expectedRow) == matchedRows.end()) {
        InputAqlItemRow missing(expected, expectedRow);
        velocypack::Options vpackOptions;
        VPackBuilder rowBuilder;
        missing.toSimpleVelocyPack(&vpackOptions, rowBuilder);
        VPackBuilder blockBuilder;
        actual->toSimpleVPack(&vpackOptions, blockBuilder);
        EXPECT_TRUE(false) << "Did not find row: " << rowBuilder.toJson()
                           << " in " << blockBuilder.toJson();
      }
    }
  }
}
