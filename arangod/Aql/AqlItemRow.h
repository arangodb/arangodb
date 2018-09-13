////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_AQL_ITEM_ROW_H
#define ARANGOD_AQL_AQL_ITEM_ROW_H 1

#include "Aql/AqlItemBlock.h"
#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

struct AqlValue;

/**
 * @brief One row within an AqlItemBlock
 *
 * Does not keep a reference to the data.
 * Caller needs to make sure that the underlying
 * AqlItemBlock is not going out of scope.
 */
class AqlItemRow {
 public:
  AqlItemRow(AqlItemBlock* block, size_t baseIndex);

  AqlItemRow(AqlItemBlock* block, size_t baseIndex, std::unordered_set<RegisterId> const& regsToKeep);

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param variableNr The register ID of the variable to read.
   *
   * @return Reference to the AqlValue stored in that variable.
   */
  const AqlValue& getValue(RegisterId variableNr) const;
  void setValue(RegisterId variableNr, AqlItemRow const& sourceRow, AqlValue const&);
  void copyRow(AqlItemRow const& sourceRow);
  std::size_t getNrRegisters() const { return _block->getNrRegs(); }
  void changeRow(std::size_t baseIndex) {
    _baseIndex = baseIndex;
    _produced = false;
  }

  void changeRow(AqlItemBlock* block, std::size_t baseIndex) {
    TRI_ASSERT(block != nullptr);
    _block = block;
    _baseIndex = baseIndex;
    _produced = false;
  }

  // returns true if row was produced
  bool produced() const { return _produced; };

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  AqlItemBlock* _block;

  /**
   * @brief The offset into the AqlItemBlock. In other words, the row's index.
   */
  size_t _baseIndex;

  std::unordered_set<RegisterId> const _regsToKeep;

  bool _produced;
};

}  // namespace aql
}  // namespace arangodb
#endif
