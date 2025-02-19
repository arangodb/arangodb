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
#include "Basics/Exceptions.h"
#include "Rest/Version.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/ExecContext.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function CURRENT_USER
AqlValue functions::CurrentUser(ExpressionContext*, AstNode const&,
                                VPackFunctionParametersView parameters) {
  std::string const& username = ExecContext::current().user();
  if (username.empty()) {
    return AqlValue(AqlValueHintNull());
  }
  return AqlValue(username);
}

/// @brief function CURRENT_DATABASE
AqlValue functions::CurrentDatabase(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  return AqlValue(expressionContext->vocbase().name());
}

/// @brief function VERSION
AqlValue functions::Version(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  return AqlValue(rest::Version::getServerVersion());
}

}  // namespace arangodb::aql
