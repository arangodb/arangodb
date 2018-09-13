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

#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

class AqlItemRow;
class AqlItemBlock;

/**
 * @brief A Matrix of AqlItemRows
 */
class AqlItemMatrix {

  public:
    AqlItemMatrix();
    ~AqlItemMatrix();

    /**
     * @brief Add this block of rows into the Matrix
     *
     * @param block Block of rows to append in the matrix
     */
    void addBlock(std::shared_ptr<AqlItemBlock> block);

    /**
     * @brief Get the number of rows stored in this Matrix
     *
     * @return Number of Rows
     */
    size_t size() const;

    /**
     * @brief Test if this matrix is empty
     *
     * @return True if empty
     */
    bool empty() const;

    /**
     * @brief Get the AqlItemRow at the given index
     *
     * @param index The index of the Row to read inside the matrix
     *
     * @return A single row in the Matrix
     */
    AqlItemRow const* getRow(size_t index) const;

  private:

    std::vector<std::shared_ptr<AqlItemBlock>> _blocks;

    size_t _size;

    // Location to keep the memory of the last
    // AQL item row. Will be mutated by getRow
    mutable std::unique_ptr<AqlItemRow const> _lastRow;
};

}  // namespace aql
}  // namespace arangodb

#endif
