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
#include "Basics/Result.h"
#include "Containers/FlatHashSet.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>

using namespace arangodb;

namespace arangodb::aql {
namespace {

/// @brief validates documents for duplicate attribute names
bool isValidDocument(VPackSlice slice) {
  slice = slice.resolveExternals();

  if (slice.isObject()) {
    containers::FlatHashSet<std::string_view> keys;

    auto it = VPackObjectIterator(slice, true);

    while (it.valid()) {
      if (!keys.emplace(it.key().stringView()).second) {
        // duplicate key
        return false;
      }

      // recurse into object values
      if (!isValidDocument(it.value())) {
        return false;
      }
      it.next();
    }
  } else if (slice.isArray()) {
    auto it = VPackArrayIterator(slice);

    while (it.valid()) {
      // recursively validate array values
      if (!isValidDocument(it.value())) {
        return false;
      }
      it.next();
    }
  }

  // all other types are considered valid
  return true;
}

/// @brief Helper function to get a document by its identifier
/// Lazily adds the collection to the transaction if necessary.
void getDocumentByIdentifier(transaction::Methods* trx,
                             OperationOptions const& options,
                             std::string& collectionName,
                             std::string const& identifier, bool ignoreError,
                             VPackBuilder& result) {
  transaction::BuilderLeaser searchBuilder(trx);

  size_t pos = identifier.find('/');
  if (pos == std::string::npos) {
    searchBuilder->add(VPackValue(identifier));
  } else {
    if (collectionName.empty()) {
      char const* p = identifier.data() + pos + 1;
      size_t l = identifier.size() - pos - 1;
      searchBuilder->add(VPackValuePair(p, l, VPackValueType::String));
      collectionName = identifier.substr(0, pos);
    } else if (identifier.compare(0, pos, collectionName) != 0) {
      // Requesting an _id that cannot be stored in this collection
      if (ignoreError) {
        return;
      }
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    } else {
      char const* p = identifier.data() + pos + 1;
      size_t l = identifier.size() - pos - 1;
      searchBuilder->add(VPackValuePair(p, l, VPackValueType::String));
    }
  }

  Result res;
  try {
    res = trx->documentFastPath(collectionName, searchBuilder->slice(), options,
                                result)
              .waitAndGet();
  } catch (basics::Exception const& ex) {
    res.reset(ex.code());
  }

  if (!res.ok()) {
    if (ignoreError) {
      if (res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) ||
          res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) ||
          res.is(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD) ||
          res.is(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD) ||
          res.is(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST)) {
        return;
      }
    }
    if (res.is(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res.errorNumber(),
          absl::StrCat(res.errorMessage(), ": ", collectionName, " [",
                       AccessMode::typeString(AccessMode::Type::READ), "]"));
    }
    THROW_ARANGO_EXCEPTION(res);
  }
}

}  // namespace

/// @brief function Document
AqlValue functions::Document(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "DOCUMENT";

  OperationOptions options;
  options.documentCallFromAql = true;

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  if (parameters.size() == 1) {
    AqlValue const& id =
        aql::functions::extractFunctionParameterValue(parameters, 0);
    transaction::BuilderLeaser builder(trx);
    if (id.isString()) {
      std::string identifier(id.slice().copyString());
      std::string colName;
      getDocumentByIdentifier(trx, options, colName, identifier, true,
                              *builder.get());
      if (builder->isEmpty()) {
        // not found
        return AqlValue(AqlValueHintNull());
      }
      return AqlValue(builder->slice(), builder->size());
    }
    if (id.isArray()) {
      AqlValueMaterializer materializer(vopts);
      VPackSlice idSlice = materializer.slice(id);
      builder->openArray();
      for (auto next : VPackArrayIterator(idSlice)) {
        if (next.isString()) {
          std::string identifier = next.copyString();
          std::string colName;
          getDocumentByIdentifier(trx, options, colName, identifier, true,
                                  *builder.get());
        }
      }
      builder->close();
      return AqlValue(builder->slice(), builder->size());
    }
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& collectionValue =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!collectionValue.isString()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }
  std::string collectionName(collectionValue.slice().copyString());

  AqlValue const& id =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  if (id.isString()) {
    transaction::BuilderLeaser builder(trx);
    std::string identifier(id.slice().copyString());
    getDocumentByIdentifier(trx, options, collectionName, identifier, true,
                            *builder.get());
    if (builder->isEmpty()) {
      return AqlValue(AqlValueHintNull());
    }
    return AqlValue(builder->slice(), builder->size());
  }

  if (id.isArray()) {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();

    AqlValueMaterializer materializer(vopts);
    VPackSlice idSlice = materializer.slice(id);
    for (auto next : VPackArrayIterator(idSlice)) {
      if (next.isString()) {
        std::string identifier(next.copyString());
        getDocumentByIdentifier(trx, options, collectionName, identifier, true,
                                *builder.get());
      }
    }

    builder->close();
    return AqlValue(builder->slice(), builder->size());
  }

  // Id has invalid format
  return AqlValue(AqlValueHintNull());
}

/// @brief function CHECK_DOCUMENT
AqlValue functions::CheckDocument(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!value.isObject()) {
    // no document at all
    return AqlValue(AqlValueHintBool(false));
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  return AqlValue(AqlValueHintBool(isValidDocument(slice)));
}

}  // namespace arangodb::aql
