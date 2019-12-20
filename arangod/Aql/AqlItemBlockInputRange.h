////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_AQLITEMBLOCKINPUTITERATOR_H
#define ARANGOD_AQL_AQLITEMBLOCKINPUTITERATOR_H

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class ShadowAqlItemRow;

class AqlItemBlockInputRange {
 public:
  explicit AqlItemBlockInputRange(ExecutorState state);

  AqlItemBlockInputRange(ExecutorState, arangodb::aql::SharedAqlItemBlockPtr const&,
                         std::size_t startIndex, std::size_t endIndex);
  AqlItemBlockInputRange(ExecutorState, arangodb::aql::SharedAqlItemBlockPtr&&,
                         std::size_t startIndex, std::size_t endIndex) noexcept;

  ExecutorState upstreamState() const noexcept;
  bool upstreamHasMore() const noexcept;

  bool hasDataRow() const noexcept;

  std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> peekDataRow();

  std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> nextDataRow();

  std::size_t getRowIndex() noexcept { return _rowIndex; };

  bool hasShadowRow() const noexcept;

  std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> peekShadowRow();

  std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> nextShadowRow();

 private:
  bool isIndexValid(std::size_t index) const noexcept;

  bool isShadowRowAtIndex(std::size_t index) const noexcept;

  enum LookAhead { NOW, NEXT };
  enum RowType { DATA, SHADOW };
  template <LookAhead doPeek, RowType type>
  ExecutorState nextState() const noexcept;

 private:
  arangodb::aql::SharedAqlItemBlockPtr _block;
  std::size_t _rowIndex;
  std::size_t _endIndex;
  ExecutorState _finalState;
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_AQLITEMBLOCKINPUTITERATOR_H
