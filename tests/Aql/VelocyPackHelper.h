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

#ifndef ARANGOD_AQL_TESTS_VELOCYPACK_HELPER_H
#define ARANGOD_AQL_TESTS_VELOCYPACK_HELPER_H

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <memory>

namespace arangodb {
namespace tests {

using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

VPackBufferPtr vpackFromJsonString(char const* c);

VPackBufferPtr operator"" _vpack(const char* json, size_t);

void VPackToAqlItemBlock(velocypack::Slice data, arangodb::aql::RegisterCount nrRegs,
                         arangodb::aql::AqlItemBlock& block);

// Convert a single VPackBuffer into an AqlItemBlock
arangodb::aql::SharedAqlItemBlockPtr vPackBufferToAqlItemBlock(
  arangodb::aql::AqlItemBlockManager& manager, VPackBufferPtr const& buffer);

/**
 * @brief Convert a list of VPackBufferPtr to a vector of AqlItemBlocks.
 * Does no error handling but for maintainer mode assertions: It's meant for
 * tests with static input.
 */
template <typename... Ts>
std::vector<arangodb::aql::SharedAqlItemBlockPtr> multiVPackBufferToAqlItemBlocks(
    arangodb::aql::AqlItemBlockManager& manager, Ts... vPackBuffers) {
  std::vector<VPackBufferPtr> buffers({std::forward<Ts>(vPackBuffers)...});
  arangodb::aql::RegisterCount const nrRegs = [&]() -> arangodb::aql::RegisterCount {
    if (buffers.empty()) {
      return 0;
    }
    for (size_t i = 0; i < buffers.size(); i++) {
      velocypack::Slice block(buffers[0]->data());
      TRI_ASSERT(block.isArray());
      if (block.length() > 0) {
        velocypack::Slice firstRow(block[0]);
        TRI_ASSERT(firstRow.isArray());
        return static_cast<arangodb::aql::RegisterCount>(firstRow.length());
      }
    }
    // no rows in any block
    return 0;
  }();

  std::vector<arangodb::aql::SharedAqlItemBlockPtr> blocks{};

  for (auto const& buffer : buffers) {
    velocypack::Slice slice(buffer->data());
    TRI_ASSERT(slice.isArray());
    size_t const nrItems = slice.length();
    arangodb::aql::SharedAqlItemBlockPtr block = nullptr;
    if (nrItems > 0) {
      block = manager.requestBlock(nrItems, nrRegs);
      VPackToAqlItemBlock(slice, nrRegs, *block);
    }
    blocks.emplace_back(block);
  }

  return blocks;
}

// Expects buffer to be an array of arrays. For every inner array, an
// AqlItemBlock with a single row matching the inner array is returned.
std::vector<arangodb::aql::SharedAqlItemBlockPtr> vPackToAqlItemBlocks(
    arangodb::aql::AqlItemBlockManager& manager, VPackBufferPtr const& buffer);

}  // namespace tests
}  // namespace arangodb

#endif  // ARANGOD_AQL_TESTS_VELOCYPACK_HELPER_H
