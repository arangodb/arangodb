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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "InsertModifier.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "ModificationExecutor2.h"
#include "ModificationExecutorTraits.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

class CollectionNameResolver;

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;

InsertModifierCompletion::InsertModifierCompletion(SimpleModifier<InsertModifierCompletion>& modifier)
    : _modifier(modifier) {}

InsertModifierCompletion::~InsertModifierCompletion() = default;

Result InsertModifierCompletion::accumulate(InputAqlItemRow& row) {
  RegisterId const inDocReg = _modifier._infos._input1RegisterId;

  // The document to be UPDATEd
  AqlValue const& inDoc = row.getValue(inDocReg);

  if (!_modifier._infos._consultAqlWriteFilter ||
      !_modifier._infos._aqlCollection->getCollection()->skipForAqlWrite(inDoc.slice(),
                                                                         StaticStrings::Empty)) {
    _modifier._accumulator.add(inDoc.slice());
    _modifier._operations.push_back({ModOperationType::APPLY_RETURN, row});
  } else {
    _modifier._operations.push_back({ModOperationType::IGNORE_RETURN, row});
  }

  return Result{};
}

Result InsertModifierCompletion::transact() {
  auto toInsert = _modifier._accumulator.slice();
  _modifier._results =
      _modifier._infos._trx->insert(_modifier._infos._aqlCollection->name(),
                                    toInsert, _modifier._infos._options);

  return Result{};
}

template class ::arangodb::aql::SimpleModifier<InsertModifierCompletion>;
