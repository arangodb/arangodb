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
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/Expression.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryExpressionContext.h"
#include "Basics/Exceptions.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>
#include <unicode/unistr.h>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function CALL
AqlValue functions::Call(ExpressionContext* expressionContext,
                         AstNode const& node,
                         VPackFunctionParametersView parameters) {
  // Since we lost JavaScript in 4.0, this can only ever error out.
  // We leave the code for potential future extensions with other
  // types of user defined functions.
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "CALL() function is not supported in this build of ArangoDB");
}

/// @brief function APPLY
AqlValue functions::Apply(ExpressionContext* expressionContext,
                          AstNode const& node,
                          VPackFunctionParametersView parameters) {
  // Since we lost JavaScript in 4.0, this can only ever error out.
  // We leave the code for potential future extensions with other
  // types of user defined functions.
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "APPLY() function is not supported in this build of ArangoDB");
}

}  // namespace arangodb::aql
