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

#include "ReplaceModifier.h"

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

ReplaceModifier::ReplaceModifier(ModificationExecutorInfos& infos)
    : _infos(infos), _resultsIterator(VPackSlice::emptyArraySlice()) {}

ReplaceModifier::~ReplaceModifier() = default;

void ReplaceModifier::reset() {
  _accumulator.clear();
  _operations.clear();
  _results = OperationResult{};
}

Result ReplaceModifier::accumulate(InputAqlItemRow& row) {
  std::string key, rev;
  Result result;

  RegisterId const inDocReg = _infos._input1RegisterId;
  RegisterId const keyReg = _infos._input2RegisterId;
  bool const hasKeyVariable = keyReg != RegisterPlan::MaxRegisterId;

  // The document to be REPLACEd
  AqlValue const& inDoc = row.getValue(inDocReg);

  // A separate register for the key/rev is available
  // so we use that
  //
  // WARNING
  //
  // We must never take _rev from the document if there is a key
  // expression.
  TRI_ASSERT(_infos._trx->resolver() != nullptr);
  CollectionNameResolver const& collectionNameResolver{*_infos._trx->resolver()};
  if (hasKeyVariable) {
    AqlValue const& keyDoc = row.getValue(keyReg);
    result = getKeyAndRevision(collectionNameResolver, keyDoc, key, rev,
                               _infos._options.ignoreRevs);
  } else {
    result = getKeyAndRevision(collectionNameResolver, inDoc, key, rev,
                               _infos._options.ignoreRevs);
  }

  if (result.ok()) {
    if (!_infos._consultAqlWriteFilter ||
        !_infos._aqlCollection->getCollection()->skipForAqlWrite(inDoc.slice(), key)) {
      if (hasKeyVariable) {
        VPackBuilder keyDocBuilder;

        buildKeyDocument(keyDocBuilder, key, rev);
        // This deletes _rev if rev is empty or ignoreRevs is set in
        // options.
        VPackCollection::merge(_accumulator, inDoc.slice(),
                               keyDocBuilder.slice(), false, true);
      } else {
        _accumulator.add(inDoc.slice());
      }
      _operations.push_back({ModOperationType::APPLY_RETURN, row});
    } else {
      _operations.push_back({ModOperationType::IGNORE_RETURN, row});
    }
  } else {
    // error happened extracting key, record in operations map
    _operations.push_back({ModOperationType::IGNORE_SKIP, row});
    // handleStats(stats, info, errorCode, _info._ignoreErrors, &errorMessage);
  }

  return Result{};
}

Result ReplaceModifier::transact() {
  auto toReplace = _accumulator.slice();
  _results =
      _infos._trx->replace(_infos._aqlCollection->name(), toReplace, _infos._options);
  return {};
}

size_t ReplaceModifier::size() const {
  // TODO: spray around some asserts
  return _accumulator.slice().length();
}

Result ReplaceModifier::setupIterator() {
  _operationsIterator = _operations.begin();
  _resultsIterator = VPackArrayIterator{_results.slice()};

  return Result{};
}

bool ReplaceModifier::isFinishedIterator() {
  // TODO: Spray in some assserts
  return _operationsIterator == _operations.end();
}

void ReplaceModifier::advanceIterator() {
  _operationsIterator++;
  _resultsIterator++;
}

// TODO: This is a bit ugly, explain at least what's going on
//       can we use local variables and just rely on the compiler not doing
//       anything silly?
// Super ugly pointer/iterator shenanigans
ReplaceModifier::OutputTuple ReplaceModifier::getOutput() {
  return OutputTuple{_operationsIterator->first, _operationsIterator->second, *_resultsIterator};
}
