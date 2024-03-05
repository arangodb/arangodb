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
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace arangodb::aql {
namespace {

template<typename T>
struct ParseBase : public T {
  explicit ParseBase(ExpressionContext* expressionContext, char const* AFN)
      : expressionContext(expressionContext), AFN(AFN) {}

  AqlValue handle(std::string_view identifier) {
    size_t pos = identifier.find('/');
    if (pos == std::string::npos ||
        identifier.find('/', pos + 1) != std::string::npos) {
      aql::functions::registerWarning(
          expressionContext, AFN,
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }

    return T::handle(identifier, pos, expressionContext);
  }

  AqlValue parse(AqlValue const& value) {
    if (value.isObject()) {
      transaction::Methods* trx = &expressionContext->trx();
      auto resolver = trx->resolver();
      TRI_ASSERT(resolver != nullptr);
      bool localMustDestroy;
      AqlValue valueStr = value.get(*resolver, StaticStrings::IdString,
                                    localMustDestroy, false);
      AqlValueGuard guard(valueStr, localMustDestroy);

      if (valueStr.isString()) {
        return this->handle(valueStr.slice().stringView());
      }
    } else if (value.isString()) {
      return this->handle(value.slice().stringView());
    }

    return this->handle("");
  }

  ExpressionContext* expressionContext;
  char const* AFN;
};

}  // namespace

/// @brief function PARSE_IDENTIFIER
AqlValue functions::ParseIdentifier(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  struct ParseIdentifierImpl {
    AqlValue handle(std::string_view identifier, size_t pos,
                    ExpressionContext* expressionContext) {
      transaction::Methods* trx = &expressionContext->trx();
      transaction::BuilderLeaser builder(trx);
      builder->openObject();
      builder->add("collection", VPackValuePair(identifier.data(), pos,
                                                VPackValueType::String));
      builder->add("key", VPackValuePair(identifier.data() + pos + 1,
                                         identifier.size() - pos - 1,
                                         VPackValueType::String));
      builder->close();
      return AqlValue(builder->slice(), builder->size());
    }
  };

  ParseBase<ParseIdentifierImpl> impl(expressionContext, "PARSE_IDENTIFIER");
  return impl.parse(
      aql::functions::extractFunctionParameterValue(parameters, 0));
}

/// @brief function PARSE_COLLECTION
AqlValue functions::ParseCollection(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  struct ParseCollectionImpl {
    AqlValue handle(std::string_view identifier, size_t pos,
                    ExpressionContext* expressionContext) {
      return AqlValue(identifier.substr(0, pos));
    }
  };

  ParseBase<ParseCollectionImpl> impl(expressionContext, "PARSE_COLLECTION");
  return impl.parse(
      aql::functions::extractFunctionParameterValue(parameters, 0));
}

/// @brief function PARSE_KEY
AqlValue functions::ParseKey(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  struct ParseKeyImpl {
    AqlValue handle(std::string_view identifier, size_t pos,
                    ExpressionContext* expressionContext) {
      return AqlValue(identifier.substr(pos + 1, identifier.size() - pos - 1));
    }
  };

  ParseBase<ParseKeyImpl> impl(expressionContext, "PARSE_KEY");
  return impl.parse(
      aql::functions::extractFunctionParameterValue(parameters, 0));
}

}  // namespace arangodb::aql
