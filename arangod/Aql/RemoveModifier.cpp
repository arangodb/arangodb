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

RemoveModifierCompletion::RemoveModifierCompletion(ModificationExecutorInfos& infos)
    : _infos(infos) {}

RemoveModifierCompletion::~RemoveModifierCompletion() = default;

ModOperationType RemoveModifierCompletion::accumulate(VPackBuilder& accu,
                                                      InputAqlItemRow& row) {
  std::string key, rev;
  Result result;

  RegisterId const inDocReg = _infos._input1RegisterId;

  // The document to be REMOVEd
  AqlValue const& inDoc = row.getValue(inDocReg);

  if (writeRequired(_infos, inDoc.slice(), StaticStrings::Empty)) {
    TRI_ASSERT(_infos._trx->resolver() != nullptr);
    CollectionNameResolver const& collectionNameResolver{*_infos._trx->resolver()};
    result = getKeyAndRevision(collectionNameResolver, inDoc, key, rev,
                               _infos._options.ignoreRevs ? Revision::Exclude
                                                          : Revision::Include);

    if (result.ok()) {
      VPackBuilder keyDocBuilder;
      buildKeyDocument(keyDocBuilder, key, rev);
      // This deletes _rev if rev is empty or
      //  ignoreRevs is set in options.
      accu.add(keyDocBuilder.slice());
      return ModOperationType::APPLY_RETURN;
    } else {
      // TODO: This is still a tad ugly. Also, what happens if there's no
      //       error message?
      if (!_infos._ignoreErrors) {
        THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
      }
      return ModOperationType::IGNORE_SKIP;
    }
  } else {
    return ModOperationType::IGNORE_RETURN;
  }
}

OperationResult RemoveModifierCompletion::transact(VPackSlice const& data) {
  return _infos._trx->remove(_infos._aqlCollection->name(), data, _infos._options);
}
