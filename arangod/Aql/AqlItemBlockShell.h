#include <utility>

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

namespace arangodb {
namespace aql {

// Deleter usable for smart pointers that return an AqlItemBlock to its manager
class AqlItemBlockDeleter {
 public:
  explicit AqlItemBlockDeleter(AqlItemBlockManager& manager_)
      : _manager(manager_) {}

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
 * Secondly, it is aware of the registers that are allowed to be read or written
 * at the current ExecutionBlock for usage with InputAqlItemRow and
 * OutputAqlItemRow.
 *
 * Note that the usage is usually *either* as an input-, *or* as an output
 * block - so one of inputRegisters or outputRegisters will be empty. The actual
 * access checks are done in the rows. If they are moved here some day, this
 * should probably get two derived classes Input- and OutputAqlItemBlockShell.
 *
 * Thirdly, it holds an InputAqlItemRow::AqlItemBlockId. This moves the id
 * nearer to the actual block and keeps InputAqlItemRow minimal. For output
 * blocks, this should be set to -1 (which is an invalid id).
 *
 * TODO We should do variable-to-register mapping here. This further reduces
 * dependencies of Executors, Fetchers etc. on internal knowledge of ItemBlocks,
 * and probably shrinks ExecutorInfos.
 *
 * TODO This should probably be split into Input- and OutputAqlItemBlockShell.
 */
class AqlItemBlockShell {
 public:
  /**
   * @brief ID type for AqlItemBlocks. Positive values are allowed, -1 means
   *        invalid/uninitialized.
   */
  using AqlItemBlockId = int64_t;

  using SmartAqlItemBlockPtr =
      std::unique_ptr<AqlItemBlock, AqlItemBlockDeleter>;

  AqlItemBlockShell(
      AqlItemBlockManager& manager, std::unique_ptr<AqlItemBlock> block_,
      std::shared_ptr<const std::unordered_set<RegisterId>> inputRegisters_,
      std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters_,
      std::shared_ptr<const std::unordered_set<RegisterId>> registersToKeep_,
      AqlItemBlockId aqlItemBlockId_);

  AqlItemBlock const& block() const { return *_block; };
  AqlItemBlock& block() { return *_block; };

  std::unordered_set<RegisterId> const& inputRegisters() const {
    return *_inputRegisters;
  };

  std::unordered_set<RegisterId> const& outputRegisters() const {
    return *_outputRegisters;
  };

  std::unordered_set<RegisterId> const& registersToKeep() const {
    return *_registersToKeep;
  };

  bool isInputRegister(RegisterId registerId) const {
    return inputRegisters().find(registerId) != inputRegisters().end();
  }

  bool isOutputRegister(RegisterId registerId) const {
    return outputRegisters().find(registerId) != outputRegisters().end();
  }

  AqlItemBlockId blockId() const {
    TRI_ASSERT(_aqlItemBlockId >= 0);
    return _aqlItemBlockId;
  }

  /**
  * @brief Steals the block, in a backwards-compatible unique_ptr. The shell
  *        is broken after this.
  */
  std::unique_ptr<AqlItemBlock> stealBlockCompat() {
    return std::unique_ptr<AqlItemBlock>(_block.release());
  }

  /**
  * @brief Compares blocks by ID. The IDs of both blocks must be valid.
  *        This is used for input blocks only, output blocks will currently not
  *        get an ID and thus may not be compared with this.
  */
  bool operator==(AqlItemBlockShell const& other) const {
    // There must be only one AqlItemBlockShell instance per AqlItemBlock,
    // and blockId() must be unique over blocks.
    TRI_ASSERT((blockId() == other.blockId()) == (this == &other));
    TRI_ASSERT((blockId() == other.blockId()) == (&block() == &other.block()));
    return blockId() == other.blockId();
  }

 private:
  SmartAqlItemBlockPtr _block;
  std::shared_ptr<const std::unordered_set<RegisterId>> _inputRegisters;
  std::shared_ptr<const std::unordered_set<RegisterId>> _outputRegisters;
  std::shared_ptr<const std::unordered_set<RegisterId>> _registersToKeep;

  /**
   * @brief Block ID. Is assumed to biuniquely identify an AqlItemBlock. Only
   *        positive values are valid. If this is set to -1, it may not be used.
   */
  AqlItemBlockId _aqlItemBlockId;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_AQL_ITEM_BLOCK_SHELL_H
