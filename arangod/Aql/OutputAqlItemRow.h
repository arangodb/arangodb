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

#include "Aql/InputAqlItemRow.h"
#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

class OutputAqlItemBlockShell;
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
  explicit OutputAqlItemRow(
      std::unique_ptr<OutputAqlItemBlockShell> blockShell);

  // Clones the given AqlValue
  void setValue(RegisterId registerId, InputAqlItemRow const& sourceRow,
                AqlValue const& value);

  // Copies the given AqlValue. If it holds external memory, it will be
  // destroyed when the block is destroyed.
  // Note that there is no real move happening here, just a trivial copy of
  // the passed AqlValue. However, that means the output block will take
  // responsibility of possibly referenced external memory.
  void setValue(RegisterId registerId, InputAqlItemRow const& sourceRow,
                AqlValue&& value);

  void copyRow(InputAqlItemRow const& sourceRow);

  std::size_t getNrRegisters() const;

  /**
   * @brief May only be called after all output values in the current row have
   * been set, or in case there are zero output registers, after copyRow has
   * been called.
   */
  void advanceRow();

  // returns true if row was produced
  bool produced() const {
    return allValuesWritten() && _inputRowCopied;
  }

  /**
  * @brief Steal the AqlItemBlock held by the OutputAqlItemRow. The returned
  *        block will contain exactly the number of written rows. e.g., if 42
  *        rows were written, block->size() will be 42, even if the original
  *        block was larger.
  *        The block will never be empty. If no rows were written, this will
  *        return a nullptr.
  *        After stealBlock(), the OutputAqlItemRow is unusable!
  */
  std::unique_ptr<AqlItemBlock> stealBlock();

  bool isFull();

  /**
  * @brief Returns the number of rows that were fully written.
  */
  size_t numRowsWritten() const noexcept {
    // If the current line was fully written, the number of fully written rows
    // is the index plus one.
    if (produced()) {
      return _baseIndex + 1;
    }

    // If the current line was not fully written, the last one was, so the
    // number of fully written rows is (_baseIndex - 1) + 1.
    return _baseIndex;

    // Disregarding unsignedness, we could also write:
    //   lastWrittenIndex = produced()
    //     ? _baseIndex
    //     : _baseIndex - 1;
    //   return lastWrittenIndex + 1;
  }

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  std::unique_ptr<OutputAqlItemBlockShell> _blockShell;

  /**
   * @brief The offset into the AqlItemBlock. In other words, the row's index.
   */
  size_t _baseIndex;

  /**
   * @brief Whether the input registers were copied from a source row.
   */
  bool _inputRowCopied;

  /**
   * @brief The last source row seen. Note that this is invalid before the first
   *        source row is seen.
   */
  InputAqlItemRow _lastSourceRow;

  /**
   * @brief Number of setValue() calls. Each entry may be written at most once,
   * so this can be used to check when all values are written.
   */
  size_t _numValuesWritten;

 private:

  size_t nextUnwrittenIndex() const noexcept {
    return numRowsWritten();
  }

  size_t numRegistersToWrite() const;

  bool allValuesWritten() const {
    return _numValuesWritten == numRegistersToWrite();
  };

  bool isOutputRegister(RegisterId regId);

  AqlItemBlock const& block() const;
  AqlItemBlock& block();
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H
