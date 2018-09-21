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
#include "Aql/InputAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

AqlItemMatrix::AqlItemMatrix(size_t nrRegs) : _size(0), _nrRegs(nrRegs) {}

void AqlItemMatrix::addBlock(std::shared_ptr<InputAqlItemBlockShell> blockShell) {
  TRI_ASSERT(blockShell->block().getNrRegs() == getNrRegisters());
  size_t blockSize = blockShell->block().size();
  _blocks.emplace_back(std::move(blockShell));
  _size += blockSize;
}

size_t AqlItemMatrix::size() const {
  return _size;
}

bool AqlItemMatrix::empty() const {
  return _blocks.empty();
}

std::unique_ptr<InputAqlItemRow> AqlItemMatrix::getRow(size_t index) const {
  TRI_ASSERT(index < _size);

  for (auto it = _blocks.begin(); it != _blocks.end() ; ++it) {
    std::shared_ptr<InputAqlItemBlockShell> blockShell = *it;
    TRI_ASSERT(blockShell != nullptr);

    if (index < blockShell->block().size()) {
        return std::make_unique<InputAqlItemRow>(blockShell, index);
    }

    // Jump over this block
    index -= blockShell->block().size();
  }
  // We have asked for a row outside of this Vector
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Internal Aql Logic Error: An executor block is reading out of bounds.");
}

