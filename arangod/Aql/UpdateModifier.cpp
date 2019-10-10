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

#include "UpdateModifier.h"

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

UpdateModifierCompletion::UpdateModifierCompletion(SimpleModifier<UpdateModifierCompletion>& modifier)
    : _modifier(modifier) {}

UpdateModifierCompletion::~UpdateModifierCompletion() = default;

Result UpdateModifierCompletion::accumulate(InputAqlItemRow& row) {
  std::string key, rev;
  Result result;

  RegisterId const inDocReg = _modifier._infos._input1RegisterId;
  RegisterId const keyReg = _modifier._infos._input2RegisterId;
  bool const hasKeyVariable = keyReg != RegisterPlan::MaxRegisterId;

  // The document to be UPDATEd
  AqlValue const& inDoc = row.getValue(inDocReg);

  // A separate register for the key/rev is available
  // so we use that
  //
  // WARNING
  //
  // We must never take _rev from the document if there is a key
  // expression.
  TRI_ASSERT(_modifier._infos._trx->resolver() != nullptr);
  CollectionNameResolver const& collectionNameResolver{*_modifier._infos._trx->resolver()};
  if (hasKeyVariable) {
    AqlValue const& keyDoc = row.getValue(keyReg);
    result = getKeyAndRevision(collectionNameResolver, keyDoc, key, rev,
                               _modifier._infos._options.ignoreRevs);
  } else {
    result = getKeyAndRevision(collectionNameResolver, inDoc, key, rev,
                               _modifier._infos._options.ignoreRevs);
  }

  if (result.ok()) {
    if (!_modifier._infos._consultAqlWriteFilter ||
        !_modifier._infos._aqlCollection->getCollection()->skipForAqlWrite(inDoc.slice(), key)) {
      if (hasKeyVariable) {
        VPackBuilder keyDocBuilder;

        buildKeyDocument(keyDocBuilder, key, rev);
        // This deletes _rev if rev is empty or ignoreRevs is set in
        // options.
        VPackCollection::merge(_modifier._accumulator, inDoc.slice(),
                               keyDocBuilder.slice(), false, true);
      } else {
        _modifier._accumulator.add(inDoc.slice());
      }
      _modifier._operations.push_back({ModOperationType::APPLY_RETURN, row});
    } else {
      _modifier._operations.push_back({ModOperationType::IGNORE_RETURN, row});
    }
  } else {
    // error happened extracting key, record in operations map
    _modifier._operations.push_back({ModOperationType::IGNORE_SKIP, row});
    // handleStats(stats, info, errorCode, _info._ignoreErrors, &errorMessage);
  }

  return Result{};
}

Result UpdateModifierCompletion::transact() {
  auto toUpdate = _modifier._accumulator.slice();
  _modifier._results =
      _modifier._infos._trx->update(_modifier._infos._aqlCollection->name(),
                                    toUpdate, _modifier._infos._options);

  return Result{};
}

template class ::arangodb::aql::SimpleModifier<UpdateModifierCompletion>;
