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

#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class AqlItemBlockInputRange {
 public:
  enum class State : uint8_t { HASMORE, DONE };

  AqlItemBlockInputRange(State, arangodb::aql::SharedAqlItemBlockPtr const&, std::size_t);
  AqlItemBlockInputRange(State, arangodb::aql::SharedAqlItemBlockPtr&&, std::size_t) noexcept;

  std::pair<State, arangodb::aql::InputAqlItemRow> peek();

  std::pair<State, arangodb::aql::InputAqlItemRow> next();

 private:
  State state() const noexcept;
  bool indexIsValid() const noexcept;
  bool moreRowsAfterThis() const noexcept;

 private:
  arangodb::aql::SharedAqlItemBlockPtr _block;
  std::size_t _rowIndex;
  State _finalState;
};

}

#endif  // ARANGOD_AQL_AQLITEMBLOCKINPUTITERATOR_H
