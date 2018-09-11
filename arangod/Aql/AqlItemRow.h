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

#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  AqlItemRow(AqlItemBlock const* block, size_t baseIndex, size_t nrRegisters);
#else
  AqlItemRow(AqlItemBlock const* block, size_t baseIndex);
#endif

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param variableNr The register ID of the variable to read.
   *
   * @return Reference to the AqlValue stored in that variable.
   */
  const AqlValue& getValue(RegisterId variableNr) const;

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  AqlItemBlock const* _block;

  /**
   * @brief The offset into the AqlItemBlock
   */
  size_t const _baseIndex;

  /**
   * @brief The number of Registers available in this Row.
   * Only relevant for sanity checks.
   * So only available in MAINTAINER_MODE.
   */
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  size_t const _nrRegisters;
#endif
};

}  // namespace aql
}  // namespace arangodb
#endif
