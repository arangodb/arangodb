////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "UpdateReplaceModifier.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorAccumulator.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Basics/Common.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"

class CollectionNameResolver;

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;

ModifierOperationType UpdateReplaceModifierCompletion::accumulate(
    ModificationExecutorAccumulator& accu, InputAqlItemRow& row) {
  RegisterId const inDocReg = _infos._input1RegisterId;
  RegisterId const keyReg = _infos._input2RegisterId;
  bool const hasKeyVariable = keyReg.isValid();

  // The document to be REPLACE/UPDATEd
  AqlValue const& inDoc = row.getValue(inDocReg);

  if (!inDoc.isObject()) {
    if (!_infos._ignoreErrors) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                                     std::string("expecting 'Object', got: ") +
                                         inDoc.slice().typeName() + std::string(" while handling: UPDATE or REPLACE"));
    }
    return ModifierOperationType::SkipRow;
  }

  // WARNING
  //
  // We must never take _rev from the document if there is a key
  // expression.
  CollectionNameResolver const& collectionNameResolver{_infos._query.resolver()};

  auto key = std::string{};
  auto rev = std::string{};

  AqlValue const& keyDoc = hasKeyVariable ? row.getValue(keyReg) : inDoc;
  Result result = getKeyAndRevision(collectionNameResolver, keyDoc, key, rev);

  if (!result.ok()) {
    // error happened extracting key, record in operations map
    if (!_infos._ignoreErrors) {
      THROW_ARANGO_EXCEPTION(result);
    }
    return ModifierOperationType::SkipRow;
  }

  if (writeRequired(_infos, inDoc.slice(), key)) {
    if (hasKeyVariable) {
      _keyDocBuilder.clear();

      if (_infos._options.ignoreRevs) {
        rev.clear();
      }

      buildKeyAndRevDocument(_keyDocBuilder, key, rev);

      // This deletes _rev if rev is empty or ignoreRevs is set in
      // options.
      auto merger =
          VPackCollection::merge(inDoc.slice(), _keyDocBuilder.slice(), false, true);
      accu.add(merger.slice());
    } else {
      accu.add(inDoc.slice());
    }
    return ModifierOperationType::ReturnIfAvailable;
  } else {
    return ModifierOperationType::CopyRow;
  }
}

OperationResult UpdateReplaceModifierCompletion::transact(transaction::Methods& trx, VPackSlice const data) {
  if (_infos._isReplace) {
    return trx.replace(_infos._aqlCollection->name(), data, _infos._options);
  } else {
    return trx.update(_infos._aqlCollection->name(), data, _infos._options);
  }
}
