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

#include "InsertModifier.h"

#include "Aql/AqlValue.h"
#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Basics/Common.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;

ModifierOperationType InsertModifierCompletion::accumulate(ModificationExecutorAccumulator& accu,
                                                           InputAqlItemRow& row) {
  RegisterId const inDocReg = _infos._input1RegisterId;

  // The document to be INSERTed
  AqlValue const& inDoc = row.getValue(inDocReg);

  if (writeRequired(_infos, inDoc.slice(), StaticStrings::Empty)) {
    accu.add(inDoc.slice());
    return ModifierOperationType::ReturnIfAvailable;
  } else {
    return ModifierOperationType::CopyRow;
  }
}

OperationResult InsertModifierCompletion::transact(transaction::Methods& trx, VPackSlice const& data) {
  return trx.insert(_infos._aqlCollection->name(), data, _infos._options);
}
