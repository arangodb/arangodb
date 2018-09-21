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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemMatrix.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockShell.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/InputAqlItemRow.h"

#include <math.h>

using namespace arangodb;
using namespace arangodb::aql;

AqlItemMatrix::AqlItemMatrix(size_t nrRegs) : _size(0), _nrRegs(nrRegs) {}

void AqlItemMatrix::addBlock(std::shared_ptr<InputAqlItemBlockShell> blockShell) {
  TRI_ASSERT(blockShell->block().getNrRegs() == getNrRegisters());
  size_t blockSize = blockShell->block().size();
  _blocks.emplace_back(_size, std::move(blockShell));
  _size += blockSize;
}

size_t AqlItemMatrix::size() const {
  return _size;
}

bool AqlItemMatrix::empty() const {
  return _blocks.empty();
}

InputAqlItemRow AqlItemMatrix::getRow(size_t index) const {
  TRI_ASSERT(index < _size);
  TRI_ASSERT(!empty());

  // Most blocks will have DefaultBatchSize
  size_t mostLikelyIndex = (std::min)(
      static_cast<size_t>(floor(index / ExecutionBlock::DefaultBatchSize())),
      _blocks.size() - 1);

  // Under the assumption that most blocks will have the DefaultBatchSize size
  // This algorithm will most likely hit the correct block in the first
  // attempt, or just one operation away.
  // If most blocks violate this condition this assumption is far of and might
  // in worst case be linear.
  while (true) {
    TRI_ASSERT(mostLikelyIndex < _blocks.size());
    auto& candidate = _blocks[mostLikelyIndex];
    TRI_ASSERT(candidate.second != nullptr);
    if (index < candidate.first) {
      // This block starts after the requested index, go left
      --mostLikelyIndex;
    } else if (index >= candidate.first + candidate.second->block().size()) {
      // This block ends before the requestsed index, go right
      ++mostLikelyIndex;
    } else {
      // Got it
      return InputAqlItemRow{candidate.second, index - candidate.first};
    }
  }

  // We have asked for a row outside of this Vector
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Internal Aql Logic Error: An executor block is reading out of bounds.");
}

