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
  friend class OutputAqlItemBlockShell;

 public:
  using SmartAqlItemBlockPtr = std::unique_ptr<AqlItemBlock, AqlItemBlockDeleter>;

  AqlItemBlock const& block() const { return *_block; };
  AqlItemBlock& block() { return *_block; };

  AqlItemBlockShell(AqlItemBlockManager& manager, std::unique_ptr<AqlItemBlock> block);

  /**
   * @brief Returns false after the block has been stolen.
   */
  bool hasBlock() const noexcept { return _block != nullptr; }

 protected:
  // Steal the block. Used in the OutputAqlItemBlockShell only.
  SmartAqlItemBlockPtr&& stealBlock() { return std::move(_block); }

 private:
  SmartAqlItemBlockPtr _block;
};


class OutputAqlItemBlockShell {
 public:
  // TODO This constructor would be able to fetch a new block itself from the
  // manager, which is needed anyway. Maybe, at least additionally, we should
  // write a constructor that takes the block dimensions instead of the block
  // itself for convenience.
  OutputAqlItemBlockShell(std::shared_ptr<AqlItemBlockShell> blockShell,
                          std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters,
                          std::shared_ptr<const std::unordered_set<RegisterId>> registersToKeep);

 public:
  std::unordered_set<RegisterId> const& outputRegisters() const {
    return *_outputRegisters;
  };

  std::unordered_set<RegisterId> const& registersToKeep() const {
    return *_registersToKeep;
  };

  bool isOutputRegister(RegisterId registerId) const {
    return outputRegisters().find(registerId) != outputRegisters().end();
  }

  /**
   * @brief Steals the block, in a backwards-compatible unique_ptr (that is,
   *        unique_ptr<AqlItemBlock> instead of
   *        unique_ptr<AqlItemBlock, AqlItemBlockDeleter>). The shell is broken
   *        after this.
   */
  std::unique_ptr<AqlItemBlock> stealBlockCompat() {
    return std::unique_ptr<AqlItemBlock>(_blockShell->stealBlock().release());
  }

  AqlItemBlock const& block() const { return _blockShell->block(); };
  AqlItemBlock& block() { return _blockShell->block(); };

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::shared_ptr<AqlItemBlockShell> const& blockShell() const {
    return _blockShell;
  }
#endif

 private:
  std::shared_ptr<AqlItemBlockShell> _blockShell;
  std::shared_ptr<const std::unordered_set<RegisterId>> _outputRegisters;
  std::shared_ptr<const std::unordered_set<RegisterId>> _registersToKeep;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_AQL_ITEM_BLOCK_SHELL_H
