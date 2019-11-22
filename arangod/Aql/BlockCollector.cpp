////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BlockCollector.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlItemBlockUtils.h"
#include "Basics/Exceptions.h"

using namespace arangodb::aql;

BlockCollector::BlockCollector(AqlItemBlockManager* blockManager)
    : _blockManager(blockManager), _totalSize(0) {}

BlockCollector::~BlockCollector() { clear(); }

size_t BlockCollector::totalSize() const { return _totalSize; }

RegisterId BlockCollector::nrRegs() const {
  TRI_ASSERT(_totalSize > 0);
  TRI_ASSERT(!_blocks.empty());
  return _blocks[0]->getNrRegs();
}

void BlockCollector::clear() {
  for (auto& it : _blocks) {
    it->destroy();  // overkill?
  }
  _blocks.clear();
  _totalSize = 0;
}

void BlockCollector::add(SharedAqlItemBlockPtr block) {
  TRI_ASSERT(block != nullptr);
  TRI_ASSERT(block->size() > 0);

  if (_blocks.capacity() == 0) {
    _blocks.reserve(8);
  }
  _blocks.push_back(block);
  _totalSize += block->size();
}

SharedAqlItemBlockPtr BlockCollector::steal() {
  if (_blocks.empty()) {
    return nullptr;
  }

  TRI_ASSERT(_totalSize > 0);
  SharedAqlItemBlockPtr result{nullptr};

  TRI_IF_FAILURE("BlockCollector::getOrSkipSomeConcatenate") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (_blocks.size() == 1) {
    // only got a single result. return it as it is
    result = _blocks[0];
  } else {
    result = itemBlock::concatenate(*_blockManager, _blocks);
    for (auto& it : _blocks) {
      it->eraseAll();
    }
  }

  // ownership is now passed to result
  _totalSize = 0;
  _blocks.clear();
  return result;
}
