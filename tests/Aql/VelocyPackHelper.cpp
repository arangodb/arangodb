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

#include "VelocyPackHelper.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;

VPackBufferPtr arangodb::tests::vpackFromJsonString(char const* c) {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();

  return builder->steal();
}

VPackBufferPtr arangodb::tests::operator"" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

void arangodb::tests::VPackToAqlItemBlock(VPackSlice data, RegisterCount nrRegs,
                                          AqlItemBlock& block) {
  // coordinates in the matrix rowNr, entryNr
  size_t rowIndex = 0;
  RegisterId entry = 0;
  for (auto const& row : VPackArrayIterator(data)) {
    // Walk through the rows
    TRI_ASSERT(row.isArray());
    TRI_ASSERT(row.length() == nrRegs);
    for (auto const& oneEntry : VPackArrayIterator(row)) {
      // Walk through on row values
      block.setValue(rowIndex, entry, AqlValue{oneEntry});
      entry++;
    }
    rowIndex++;
    entry = 0;
  }
}

arangodb::aql::SharedAqlItemBlockPtr arangodb::tests::vPackBufferToAqlItemBlock(
    arangodb::aql::AqlItemBlockManager& manager, VPackBufferPtr const& buffer) {
  if(VPackSlice(buffer->data()).isNone()) {
    return nullptr;
  }
  return multiVPackBufferToAqlItemBlocks(manager, buffer)[0];
}

std::vector<SharedAqlItemBlockPtr> arangodb::tests::vPackToAqlItemBlocks(
    AqlItemBlockManager& manager, VPackBufferPtr const& buffer) {
  VPackSlice outer(buffer->data());
  if (outer.isNone()) {
    return {};
  }
  TRI_ASSERT(outer.isArray());
  if (outer.length() == 0) {
    return {};
  }
  RegisterCount const nrRegs = [&]() {
    VPackSlice firstRow(outer[0]);
    TRI_ASSERT(firstRow.isArray());
    return static_cast<RegisterCount>(firstRow.length());
  }();

  auto wrap = [](VPackSlice slice) -> VPackBufferPtr {
    VPackBuilder builder;
    builder.openArray();
    builder.add(slice);
    builder.close();
    return builder.steal();
  };

  std::vector<SharedAqlItemBlockPtr> blocks;

  for (VPackSlice inner : VPackArrayIterator(outer)) {
    SharedAqlItemBlockPtr block = manager.requestBlock(1, nrRegs);
    VPackToAqlItemBlock(VPackSlice(wrap(inner)->data()), nrRegs, *block);
    blocks.emplace_back(block);
  }

  return blocks;
}
