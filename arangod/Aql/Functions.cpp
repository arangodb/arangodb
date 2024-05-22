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

#include "Functions.h"

#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <absl/strings/str_cat.h>

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

/// @brief an empty AQL value
static AqlValue const emptyAqlValue;

}  // namespace

/// @brief extract a boolean parameter from an array
bool functions::getBooleanParameter(
    aql::functions::VPackFunctionParametersView parameters,
    size_t startParameter, bool defaultValue) {
  size_t const n = parameters.size();

  if (startParameter >= n) {
    return defaultValue;
  }

  return parameters[startParameter].toBoolean();
}

AqlValue const& functions::extractFunctionParameterValue(
    aql::functions::VPackFunctionParametersView parameters, size_t position) {
  if (position >= parameters.size()) {
    // parameter out of range
    return ::emptyAqlValue;
  }
  return parameters[position];
}

std::string_view functions::getFunctionName(AstNode const& node) noexcept {
  TRI_ASSERT(aql::NODE_TYPE_FCALL == node.type);
  auto const* impl = static_cast<aql::Function*>(node.getData());
  TRI_ASSERT(impl != nullptr);
  return impl->name;
}

/// @brief register warning
void functions::registerWarning(ExpressionContext* expressionContext,
                                std::string_view functionName,
                                Result const& r) {
  std::string msg =
      absl::StrCat("in function '", functionName, "()': ", r.errorMessage());
  expressionContext->registerWarning(r.errorNumber(), msg);
}

/// @brief register warning
void functions::registerWarning(ExpressionContext* expressionContext,
                                std::string_view functionName, ErrorCode code) {
  if (code != TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH &&
      code != TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH) {
    registerWarning(expressionContext, functionName, Result(code));
    return;
  }

  // ensure that function name is null-terminated
  // TODO: if we get rid of vsprintf in FillExceptionString, we don't
  // need to pass a raw C-style string into it
  std::string fname(functionName);
  std::string msg = basics::Exception::FillExceptionString(code, fname.c_str());
  expressionContext->registerWarning(code, msg);
}

void functions::registerError(ExpressionContext* expressionContext,
                              std::string_view functionName, ErrorCode code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    // ensure that function name is null-terminated
    // TODO: if we get rid of vsprintf in FillExceptionString, we don't
    // need to pass a raw C-style string into it
    std::string fname(functionName);
    msg = basics::Exception::FillExceptionString(code, fname.c_str());
  } else {
    msg = absl::StrCat("in function '", functionName,
                       "()': ", TRI_errno_string(code));
  }

  expressionContext->registerError(code, msg);
}

/// @brief register usage of an invalid function argument
void functions::registerInvalidArgumentWarning(
    ExpressionContext* expressionContext, std::string_view functionName) {
  aql::functions::registerWarning(
      expressionContext, functionName,
      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

/// @brief convert a number value into an AqlValue
AqlValue functions::numberValue(double value, bool nullify) {
  if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL ||
      value == -HUGE_VAL) {
    if (nullify) {
      // convert to null
      return AqlValue(AqlValueHintNull());
    }
    // convert to 0
    value = 0.0;
  }

  return AqlValue(AqlValueHintDouble(value));
}

/// @brief extra a collection name from an AqlValue
std::string functions::extractCollectionName(
    transaction::Methods* trx,
    aql::functions::VPackFunctionParametersView parameters, size_t position) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, position);

  std::string identifier;

  if (value.isString()) {
    // already a string
    identifier = value.slice().copyString();
  } else {
    AqlValueMaterializer materializer(&trx->vpackOptions());
    VPackSlice slice = materializer.slice(value);
    VPackSlice id = slice;

    if (slice.isObject()) {
      id = slice.get(StaticStrings::IdString);
    }
    if (id.isString()) {
      identifier = id.copyString();
    } else if (id.isCustom()) {
      identifier = trx->extractIdString(slice);
    }
  }

  if (!identifier.empty()) {
    size_t pos = identifier.find('/');

    if (pos != std::string::npos) {
      // this is superior to  identifier.substr(0, pos)
      identifier.resize(pos);
    }

    return identifier;
  }

  return StaticStrings::Empty;
}

/// @brief append the VelocyPack value to a string buffer
///        Note: Backwards compatibility. Is different than Slice.toJson()
template<typename T>
void functions::appendAsString(velocypack::Options const& vopts, T& buffer,
                               AqlValue const& value) {
  AqlValueMaterializer materializer(&vopts);
  VPackSlice slice = materializer.slice(value);

  functions::stringify(&vopts, buffer, slice);
}

template void functions::appendAsString<arangodb::velocypack::StringSink>(
    velocypack::Options const& vopts, arangodb::velocypack::StringSink& buffer,
    AqlValue const& value);

template void
functions::appendAsString<arangodb::velocypack::SizeConstrainedStringSink>(
    velocypack::Options const& vopts,
    arangodb::velocypack::SizeConstrainedStringSink& buffer,
    AqlValue const& value);

/// @brief append the VelocyPack value to a string buffer
template<typename T>
void functions::stringify(VPackOptions const* vopts, T& buffer,
                          VPackSlice slice) {
  if (slice.isNull()) {
    // null is the empty string
    return;
  }

  if (slice.isString()) {
    // dumping adds additional ''. we want to avoid that.
    VPackValueLength length;
    char const* p = slice.getStringUnchecked(length);
    buffer.append(p, length);
    return;
  }

  VPackOptions adjustedOptions = *vopts;
  adjustedOptions.escapeUnicode = false;
  adjustedOptions.escapeForwardSlashes = false;
  velocypack::Dumper dumper(&buffer, &adjustedOptions);
  dumper.dump(slice);
}

template void functions::stringify<arangodb::velocypack::StringSink>(
    velocypack::Options const* vopts, arangodb::velocypack::StringSink& buffer,
    arangodb::velocypack::Slice slice);

template void
functions::stringify<arangodb::velocypack::SizeConstrainedStringSink>(
    velocypack::Options const* vopts,
    arangodb::velocypack::SizeConstrainedStringSink& buffer,
    arangodb::velocypack::Slice slice);
