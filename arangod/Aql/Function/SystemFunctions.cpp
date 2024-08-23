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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <chrono>
#include <thread>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function SLEEP
AqlValue functions::Sleep(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isNumber() || value.toDouble() < 0) {
    registerWarning(expressionContext, "SLEEP",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  auto& server = expressionContext->vocbase().server();

  double const sleepValue = value.toDouble();

  if (sleepValue < 0.010) {
    // less than 10 ms sleep time
    std::this_thread::sleep_for(std::chrono::duration<double>(sleepValue));

    if (expressionContext->killed()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
    } else if (server.isStopping()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
  } else {
    auto now = std::chrono::steady_clock::now();
    auto const endTime = now + std::chrono::milliseconds(
                                   static_cast<int64_t>(sleepValue * 1000.0));

    while (now < endTime) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      if (expressionContext->killed()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
      } else if (server.isStopping()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }
      now = std::chrono::steady_clock::now();
    }
  }

  return AqlValue(AqlValueHintNull());
}

}  // namespace arangodb::aql
