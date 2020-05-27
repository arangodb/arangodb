////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DocumentExpressionContext.h"

#include "Aql/AqlValue.h"

using namespace arangodb::aql;

DocumentExpressionContext::DocumentExpressionContext(arangodb::transaction::Methods& trx,
                                                     QueryContext& query,
                                                     AqlFunctionsInternalCache& cache,
                                                     arangodb::velocypack::Slice document)
    : QueryExpressionContext(trx, query, cache), _document(document) {}

AqlValue DocumentExpressionContext::getVariableValue(Variable const*, bool doCopy,
                                                     bool& mustDestroy) const {
  if (doCopy) {
    mustDestroy = true;  // as we are copying
    return AqlValue(AqlValueHintCopy(_document.start()));
  }
  mustDestroy = false;
  return AqlValue(AqlValueHintDocumentNoCopy(_document.start()));
}
