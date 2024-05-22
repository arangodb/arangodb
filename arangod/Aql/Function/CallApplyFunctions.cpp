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
#ifdef USE_V8
#include "V8/v8-vpack.h"
#endif
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>
#include <unicode/unistr.h>

#ifdef USE_V8
#include <v8.h>
#endif

using namespace arangodb;

namespace arangodb::aql {

#ifdef USE_V8
AqlValue callApplyBackend(
    ExpressionContext* expressionContext, AstNode const& node, char const* AFN,
    AqlValue const& invokeFN,
    aql::functions::VPackFunctionParametersView invokeParams) {
  auto& trx = expressionContext->trx();

  std::string ucInvokeFN;
  transaction::StringLeaser buffer(&trx);
  velocypack::StringSink adapter(buffer.get());

  aql::functions::appendAsString(trx.vpackOptions(), adapter, invokeFN);

  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));
  unicodeStr.toUpper();
  unicodeStr.toUTF8String(ucInvokeFN);

  aql::Function const* func = nullptr;
  if (ucInvokeFN.find("::") == std::string::npos) {
    // built-in C++ function
    auto& server = trx.vocbase().server();
    func = server.getFeature<AqlFunctionFeature>().byName(ucInvokeFN);
    if (func->hasCxxImplementation()) {
      std::pair<size_t, size_t> numExpectedArguments = func->numArguments();

      if (invokeParams.size() < numExpectedArguments.first ||
          invokeParams.size() > numExpectedArguments.second) {
        THROW_ARANGO_EXCEPTION_PARAMS(
            TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH,
            ucInvokeFN.c_str(), static_cast<int>(numExpectedArguments.first),
            static_cast<int>(numExpectedArguments.second));
      }

      return func->implementation(expressionContext, node, invokeParams);
    }
  }

  // JavaScript function (this includes user-defined functions)
  {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          absl::StrCat(
              "no V8 context available when executing call to function ", AFN));
    }

    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
        isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));

    TRI_ASSERT(v8g != nullptr);

    auto queryCtx = dynamic_cast<QueryExpressionContext*>(expressionContext);
    if (queryCtx == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "unable to cast into QueryExpressionContext");
    }

    auto query = dynamic_cast<Query*>(&queryCtx->query());
    if (query == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unable to cast into Query");
    }

    VPackOptions const& options = trx.vpackOptions();

    auto old = v8g->_expressionContext;
    v8g->_expressionContext = expressionContext;
    auto sg = scopeGuard([&]() noexcept { v8g->_expressionContext = old; });

    AqlValue funcRes;
    query->runInV8ExecutorContext([&](v8::Isolate* isolate) {
      v8::HandleScope scope(isolate);

      std::string jsName;
      int const n = static_cast<int>(invokeParams.size());
      int const callArgs = (func == nullptr ? 3 : n);
      auto args = std::make_unique<v8::Handle<v8::Value>[]>(callArgs);

      if (func == nullptr) {
        // a call to a user-defined function
        jsName = "FCALL_USER";

        // function name
        args[0] = TRI_V8_STD_STRING(isolate, ucInvokeFN);

        v8::Handle<v8::Context> context = isolate->GetCurrentContext();
        // call parameters
        v8::Handle<v8::Array> params =
            v8::Array::New(isolate, static_cast<int>(n));

        for (int i = 0; i < n; ++i) {
          params
              ->Set(context, static_cast<uint32_t>(i),
                    invokeParams[i].toV8(isolate, &options))
              .FromMaybe(true);
        }
        args[1] = params;
        args[2] = TRI_V8_ASCII_STRING(isolate, AFN);
      } else {
        // a call to a built-in V8 function
        TRI_ASSERT(func->hasV8Implementation());

        jsName = absl::StrCat("AQL_", func->name);
        for (int i = 0; i < n; ++i) {
          args[i] = invokeParams[i].toV8(isolate, &options);
        }
      }

      bool dummy;
      funcRes = Expression::invokeV8Function(*expressionContext, jsName,
                                             isolate, ucInvokeFN, AFN, false,
                                             callArgs, args.get(), dummy);
    });
    return funcRes;
  }
}
#endif

/// @brief function CALL
AqlValue functions::Call(ExpressionContext* expressionContext,
                         AstNode const& node,
                         VPackFunctionParametersView parameters) {
#ifdef USE_V8
  static char const* AFN = "CALL";

  AqlValue const& invokeFN =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!invokeFN.isString()) {
    registerError(expressionContext, AFN,
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  VPackFunctionParameters invokeParams;
  if (parameters.size() >= 2) {
    // we have a list of parameters, need to copy them over except the
    // functionname:
    invokeParams.reserve(parameters.size() - 1);

    for (uint64_t i = 1; i < parameters.size(); i++) {
      invokeParams.push_back(
          aql::functions::extractFunctionParameterValue(parameters, i));
    }
  }

  return callApplyBackend(expressionContext, node, AFN, invokeFN, invokeParams);
#else
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "CALL() function is not supported in this build of ArangoDB");
#endif
}

/// @brief function APPLY
AqlValue functions::Apply(ExpressionContext* expressionContext,
                          AstNode const& node,
                          VPackFunctionParametersView parameters) {
#ifdef USE_V8
  static char const* AFN = "APPLY";

  AqlValue const& invokeFN =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!invokeFN.isString()) {
    registerError(expressionContext, AFN,
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  VPackFunctionParameters invokeParams;
  AqlValue rawParamArray;
  std::vector<bool> mustFree;

  auto guard = scopeGuard([&mustFree, &invokeParams]() noexcept {
    for (size_t i = 0; i < mustFree.size(); ++i) {
      if (mustFree[i]) {
        invokeParams[i].destroy();
      }
    }
  });

  if (parameters.size() == 2) {
    // We have a parameter that should be an array, whichs content we need to
    // make the sub functions parameters.
    rawParamArray =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (!rawParamArray.isArray()) {
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    uint64_t len = rawParamArray.length();
    invokeParams.reserve(len);
    mustFree.reserve(len);
    for (uint64_t i = 0; i < len; i++) {
      bool f;
      invokeParams.push_back(rawParamArray.at(i, f, false));
      mustFree.push_back(f);
    }
  }

  return callApplyBackend(expressionContext, node, AFN, invokeFN, invokeParams);
#else
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "APPLY() function is not supported in this build of ArangoDB");
#endif
}

}  // namespace arangodb::aql
