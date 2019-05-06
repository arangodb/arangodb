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

#ifndef ARANGOD_AQL_AQL_ITEM_MATRIX_H
#define ARANGOD_AQL_AQL_ITEM_MATRIX_H 1

#include <lib/Logger/Logger.h>
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/types.h"
#include "Basics/Common.h"

#include <math.h>

namespace arangodb {
namespace aql {

/**
 * @brief A Matrix of AqlItemRows
 */
class AqlItemMatrix {
 public:
  explicit AqlItemMatrix(size_t nrRegs) : _size(0), _nrRegs(nrRegs) {}
  ~AqlItemMatrix() = default;

  /**
   * @brief Add this block of rows into the Matrix
   *
   * @param blockPtr Block of rows to append in the matrix
   */
  void addBlock(SharedAqlItemBlockPtr blockPtr) {
    TRI_ASSERT(blockPtr->getNrRegs() == getNrRegisters());
    size_t blockSize = blockPtr->size();
    _blocks.emplace_back(_size, std::move(blockPtr));
    _size += blockSize;
  }

  /**
   * @brief Get the number of rows stored in this Matrix
   *
   * @return Number of Rows
   */
  size_t size() const { return _size; }

  /**
   * @brief Number of registers, i.e. width of the matrix.
   */
  size_t getNrRegisters() const { return _nrRegs; };

  /**
   * @brief Test if this matrix is empty
   *
   * @return True if empty
   */
  bool empty() const { return _blocks.empty(); }

  /**
   * @brief Get the AqlItemRow at the given index
   *
   * @param index The index of the Row to read inside the matrix
   *
   * @return A single row in the Matrix
   */
  InputAqlItemRow getRow(size_t index) const {
    TRI_ASSERT(index < _size);
    TRI_ASSERT(!empty());

    // Most blocks will have DefaultBatchSize
    size_t mostLikelyIndex =
        (std::min)(static_cast<size_t>(floor(index / ExecutionBlock::DefaultBatchSize())),
                   _blocks.size() - 1);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    size_t iterations = 0;
#endif
    size_t minIndex = 0;
    size_t maxIndex = _blocks.size() - 1;

    // Under the assumption that all but the last block will have the
    // DefaultBatchSize size, this algorithm will hit the correct block in the
    // first attempt.
    // As a fallback, it does a binary search on the blocks.
    while (true) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      ++iterations;
      TRI_ASSERT(iterations < _blocks.size() * 2);
#endif
      TRI_ASSERT(minIndex <= maxIndex);
      TRI_ASSERT(maxIndex < _blocks.size());
      TRI_ASSERT(mostLikelyIndex <= maxIndex);
      TRI_ASSERT(mostLikelyIndex >= minIndex);
      auto& candidate = _blocks[mostLikelyIndex];
      TRI_ASSERT(candidate.second != nullptr);
      if (index < candidate.first) {
        // This block starts after the requested index, go left.
        // Assert that there is a left to go to. This could only go wrong if the
        // candidate.first values are wrong.
        TRI_ASSERT(mostLikelyIndex > 0);
        // To assure yourself of the correctness, remember that / rounds down and
        // 0 <= mostLikelyIndex / 2 < mostLikelyIndex
        // Narrow down upper Border
        maxIndex = mostLikelyIndex;

        mostLikelyIndex = minIndex + (mostLikelyIndex - minIndex) / 2;
      } else if (index >= candidate.first + candidate.second->size()) {
        minIndex = mostLikelyIndex;
        // This block ends before the requested index, go right.
        // Assert that there is a right to go to. This could only go wrong if
        // the candidate.first values are wrong.
        TRI_ASSERT(mostLikelyIndex < _blocks.size() - 1);
        // To assure yourself of the correctness, remember that / rounds down,
        // numBlocksRightFromHere >= 1 (see assert above) and
        //   0
        //   <= numBlocksRightFromHere / 2
        //   < numBlocksRightFromHere
        // . Therefore
        //   mostLikelyIndex
        //   < mostLikelyIndex + 1 + numBlocksRightFromHere / 2
        //   < _blocks.size()
        // .
        //
        // Please do not bother to shorten the following expression, the
        // compilers are perfectly capable of doing that, and here the
        // correctness is easier to see.
        size_t const numBlocksRightFromHere = maxIndex - mostLikelyIndex;
        mostLikelyIndex += 1 + numBlocksRightFromHere / 2;
      } else {
#if 0
        LOG_TOPIC_IF("c8c68", WARN, Logger::AQL, iterations > 1)
            << "Suboptimal AqlItemMatrix index lookup: Did " << iterations
            << " iterations.";
#endif
        // Got it
        return InputAqlItemRow{candidate.second, index - candidate.first};
      }
    }

    // We have asked for a row outside of this Vector
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Internal Aql Logic Error: An executor "
                                   "block is reading out of bounds.");
  }

  inline size_t numberOfBlocks() const { return _blocks.size(); }

  inline SharedAqlItemBlockPtr getBlock(size_t index) {
    TRI_ASSERT(index < numberOfBlocks());
    return _blocks.at(index).second;
  }

 private:
  std::vector<std::pair<size_t, SharedAqlItemBlockPtr>> _blocks;

  size_t _size;

  size_t _nrRegs;
};

}  // namespace aql
}  // namespace arangodb

#endif
