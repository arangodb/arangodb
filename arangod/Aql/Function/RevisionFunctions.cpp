////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/system-functions.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <ctime>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function DECODE_REV
AqlValue functions::DecodeRev(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  auto const rev = aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!rev.isString()) {
    registerInvalidArgumentWarning(expressionContext, "DECODE_REV");
    return AqlValue(AqlValueHintNull());
  }

  VPackValueLength l;
  char const* p = rev.slice().getString(l);
  uint64_t revInt = basics::HybridLogicalClock::decodeTimeStamp(p, l);

  if (revInt == 0 || revInt == UINT64_MAX) {
    registerInvalidArgumentWarning(expressionContext, "DECODE_REV");
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();

  uint64_t timeMilli = basics::HybridLogicalClock::extractTime(revInt);
  uint64_t count = basics::HybridLogicalClock::extractCount(revInt);
  time_t timeSeconds = timeMilli / 1000;
  uint64_t millis = timeMilli % 1000;
  struct tm date;
  TRI_gmtime(timeSeconds, &date);

  char buffer[32];
  strftime(buffer, 32, "%Y-%m-%dT%H:%M:%S.000Z", &date);
  // fill millisecond part not covered by strftime
  buffer[20] = static_cast<char>(millis / 100) + '0';
  buffer[21] = ((millis / 10) % 10) + '0';
  buffer[22] = (millis % 10) + '0';
  // buffer[23] is 'Z'
  buffer[24] = 0;

  transaction::BuilderLeaser builder(trx);
  builder->openObject();
  builder->add("date", VPackValue(buffer));
  builder->add("count", VPackValue(count));
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

}  // namespace arangodb::aql
