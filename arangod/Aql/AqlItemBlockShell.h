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

#ifndef ARANGOD_AQL_AQL_ITEM_BLOCK_SHELL_H
#define ARANGOD_AQL_AQL_ITEM_BLOCK_SHELL_H

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"

#include <memory>
#include <utility>

namespace arangodb {
namespace aql {

// Deleter usable for smart pointers that return an AqlItemBlock to its manager
class AqlItemBlockDeleter {
 public:
  explicit AqlItemBlockDeleter(AqlItemBlockManager& manager)
      : _manager(manager) {}

  void operator()(AqlItemBlock* block) { _manager.returnBlock(block); }

 private:
  AqlItemBlockManager& _manager;
};

/**
 * @brief This class is a decorator for AqlItemBlock.
 *
 * For one thing, it automatically returns the AqlItemBlock to the
 * AqlItemBlockManager upon destruction. If only this is needed, it would also
 * suffice to use the AqlItemBlockDeleter above and pass it as custom deleter
 * to a unique_ptr or shared_ptr.
 *
 * Secondly, Input- and OutputAqlItemBlockShell are aware of the registers that
 * are allowed to be read or written at the current ExecutionBlock, for usage
 * with InputAqlItemRow or OutputAqlItemRow, respectively.
 *
 * Thirdly, it allows an Input- and OutputAqlItemBlockShell to share a single
 * AqlItemBlock at the same time. This is used for ExecutionBlocks that do not
 * change the number of rows, like CalculationBlock, to reuse the input blocks
 * instead of copying them.
 *
 * TODO We should do variable-to-register mapping here. This further reduces
 * dependencies of Executors, Fetchers etc. on internal knowledge of ItemBlocks,
 * and probably shrinks ExecutorInfos.
 */
class AqlItemBlockShell {
 public:
  using SmartAqlItemBlockPtr = std::unique_ptr<AqlItemBlock, AqlItemBlockDeleter>;

  AqlItemBlock const& block() const { return *_block; };
  AqlItemBlock& block() { return *_block; };

  AqlItemBlockShell(AqlItemBlockManager& manager, std::unique_ptr<AqlItemBlock> block);

  /**
   * @brief Returns false after the block has been stolen.
   */
  bool hasBlock() const noexcept { return _block != nullptr; }

  // Steal the block. Breaks the shell!
  SmartAqlItemBlockPtr stealBlock() { return std::move(_block); }
  std::unique_ptr<AqlItemBlock> stealBlockCompat() {
    return std::unique_ptr<AqlItemBlock>(stealBlock().release());
  }

 private:
  SmartAqlItemBlockPtr _block;
};


}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_AQL_ITEM_BLOCK_SHELL_H
