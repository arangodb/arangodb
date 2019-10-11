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

#include "RemoveModifier.h"

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

RemoveModifierCompletion::RemoveModifierCompletion(SimpleModifier<RemoveModifierCompletion>& modifier)
    : _modifier(modifier) {}

RemoveModifierCompletion::~RemoveModifierCompletion() = default;

ModOperationType RemoveModifierCompletion::accumulate(InputAqlItemRow& row) {
  std::string key, rev;
  Result result;

  RegisterId const inDocReg = _modifier.getInfos()._input1RegisterId;

  // The document to be REMOVEd
  AqlValue const& inDoc = row.getValue(inDocReg);

  if (_modifier.writeRequired(inDoc.slice(), StaticStrings::Empty)) {
    TRI_ASSERT(_modifier.getInfos()._trx->resolver() != nullptr);
    CollectionNameResolver const& collectionNameResolver{*_modifier._infos._trx->resolver()};
    result = getKeyAndRevision(collectionNameResolver, inDoc, key, rev,
                               _modifier._infos._options.ignoreRevs);

    if (result.ok()) {
      VPackBuilder keyDocBuilder;
      buildKeyDocument(keyDocBuilder, key, rev);
      // This deletes _rev if rev is empty or ignoreRevs is set in
      // options.
      _modifier.addDocument(keyDocBuilder.slice());
      return ModOperationType::APPLY_RETURN;
    } else {
      // error happened extracting key, record in operations map
      THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
      return ModOperationType::IGNORE_SKIP;
    }
  } else {
    return ModOperationType::IGNORE_RETURN;
  }
}

OperationResult RemoveModifierCompletion::transact() {
  auto toRemove = _modifier._accumulator.slice();
  return _modifier.getInfos()._trx->remove(_modifier.getInfos()._aqlCollection->name(),
                                           toRemove, _modifier.getInfos()._options);
}
