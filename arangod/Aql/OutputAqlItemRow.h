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
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H
#define ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H

#include "Aql/AqlItemBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

struct AqlValue;

/**
 * @brief One row within an AqlItemBlock, for writing.
 *
 * Does not keep a reference to the data.
 * Caller needs to make sure that the underlying
 * AqlItemBlock is not going out of scope.
 */
class OutputAqlItemRow {
 public:
  OutputAqlItemRow(std::unique_ptr<AqlItemBlock> block,
                   std::unordered_set<RegisterId> const& regsToKeep);

  void setValue(RegisterId variableNr, InputAqlItemRow const& sourceRow,
                AqlValue const&);

  void copyRow(InputAqlItemRow const& sourceRow);

  std::size_t getNrRegisters() const { return _block->getNrRegs(); }

  /// @deprecated Is unneeded, advanceRow() suffices.
  void changeRow(std::size_t baseIndex) {
    _baseIndex = baseIndex;
    _currentRowIsComplete = false;
  }

  void advanceRow() {
    ++_baseIndex;
    _currentRowIsComplete = false;
  }

  // returns true if row was produced
  bool produced() const { return _currentRowIsComplete; };

  std::unique_ptr<AqlItemBlock> stealBlock() {
    if (numRowsWritten() == 0) {
      // blocks may not be empty
      _block.reset(nullptr);
    } else {
      _block->shrink(numRowsWritten());
    }
    return std::move(_block);
  }

  bool isFull() { return numRowsWritten() >= _block->size(); }

  size_t numRowsWritten() const noexcept {
    if (_currentRowIsComplete) {
      return _baseIndex + 1;
    }

    return _baseIndex;
  }

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  std::unique_ptr<AqlItemBlock> _block;

  /**
   * @brief The offset into the AqlItemBlock. In other words, the row's index.
   */
  size_t _baseIndex;

  std::unordered_set<RegisterId> const _regsToKeep;

  bool _currentRowIsComplete;

  /**
  * @brief The last source row seen. Note that this is invalid before the first
  *        source row is seen.
  */
  InputAqlItemRow _lastSourceRow;

 private:

  size_t nextUnwrittenIndex() const noexcept {
    return numRowsWritten();
  }
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H
