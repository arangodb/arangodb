////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AqlHelper.h"

#include "Aql/ExecutionStats.h"
#include "Basics/VelocyPackHelper.h"

using namespace arangodb;
using namespace arangodb::aql;

std::ostream& arangodb::aql::operator<<(std::ostream& stream, ExecutionStats const& stats) {
  VPackBuilder builder{};
  stats.toVelocyPack(builder, true);
  return stream << builder.toJson();
}

std::ostream& arangodb::aql::operator<<(std::ostream& stream, AqlItemBlock const& block) {
  stream << "[";
  for (size_t row = 0; row < block.numRows(); row++) {
    if (row > 0) {
      stream << ",";
    }
    stream << " ";
    VPackBuilder builder{};
    builder.openArray();
    for (RegisterId::value_t reg = 0; reg < block.numRegisters(); reg++) {
      if (reg > 0) {
        stream << ",";
      }
      // will not work for docvecs or ranges
      builder.add(block.getValueReference(row, reg).slice());
    }
    builder.close();
    stream << builder.toJson();
  }
  stream << " ]";
  return stream;
}

bool arangodb::aql::operator==(arangodb::aql::ExecutionStats const& left,
                               arangodb::aql::ExecutionStats const& right) {
  // The below information is only set on profiling in AQL
  // They are not included on purpose as they will never be equal.
  // * nodes
  // * executionTime
  // * peakMemeoryUsage

  // clang-format off
  return left.writesExecuted == right.writesExecuted
      && left.writesIgnored == right.writesIgnored
      && left.scannedFull == right.scannedFull
      && left.scannedIndex == right.scannedIndex
      && left.filtered == right.filtered
      && left.requests == right.requests
      && left.fullCount == right.fullCount
      && left.count == right.count;
  // clang-format on
}
bool arangodb::aql::operator==(arangodb::aql::AqlItemBlock const& left,
                               arangodb::aql::AqlItemBlock const& right) {
  if (left.numRows() != right.numRows()) {
    return false;
  }
  if (left.numRegisters() != right.numRegisters()) {
    return false;
  }
  size_t const rows = left.numRows();
  RegisterCount const regs = left.numRegisters();
  for (size_t row = 0; row < rows; row++) {
    for (RegisterId::value_t reg = 0; reg < regs; reg++) {
      AqlValue const& l = left.getValueReference(row, reg);
      AqlValue const& r = right.getValueReference(row, reg);
      // Doesn't work for docvecs or ranges
      if (arangodb::basics::VelocyPackHelper::compare(l.slice(), r.slice(), false) != 0) {
        return false;
      }
    }
  }

  return true;
}
