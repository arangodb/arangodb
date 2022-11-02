////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <absl/strings/str_cat.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFilterContext.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "RestServer/arangod.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"

#include "search/all_filter.hpp"
#include "search/column_existence_filter.hpp"

using namespace std::string_literals;

namespace arangodb::iresearch {

class ByExpression;

irs::filter::ptr makeAll(std::string_view field);

std::string_view makeAllColumn(QueryContext const& ctx) noexcept;

irs::AllDocsProvider::ProviderFunc makeAllProvider(QueryContext const& ctx);

irs::ColumnAcceptor makeColumnAcceptor(bool) noexcept;

template<typename Filter, typename Source>
auto& append(Source& parent, [[maybe_unused]] FilterContext const& ctx) {
  if constexpr (std::is_same_v<Filter, irs::all>) {
    static_assert(std::is_base_of_v<irs::boolean_filter, Source>);
#ifdef USE_ENTERPRISE
    return parent.add(makeAll(makeAllColumn(ctx.query)));
#else
    return parent.template add<irs::all>();
#endif
  } else {
    auto* filter = [&] {
      if constexpr (std::is_same_v<irs::Not, Source>) {
        return &parent.template filter<Filter>();
      } else {
        return &parent.template add<Filter>();
      }
    }();

#ifdef USE_ENTERPRISE
    if constexpr (std::is_base_of_v<irs::AllDocsProvider, Filter>) {
      filter->SetProvider(makeAllProvider(ctx.query));
    }
#endif

    return *filter;
  }
}

template<typename Filter, typename Source>
Filter& appendNot(Source& parent, FilterContext const& ctx) {
  return append<Filter>(
      append<irs::Not>(parent.type() == irs::type<irs::Or>::id()
                           ? append<irs::And>(parent, ctx)
                           : parent,
                       ctx),
      ctx);
}

namespace error {

template<size_t Min, size_t Max>
struct Range {
  static constexpr size_t MIN = Min;
  static constexpr size_t MAX = Max;
};

template<typename>
struct IsRange : std::false_type {};
template<size_t Min, size_t Max>
struct IsRange<Range<Min, Max>> : std::true_type {};

template<bool MaxBound, size_t Value>
struct OpenRange {
  static constexpr bool MAX_BOUND = MaxBound;
  static constexpr size_t VALUE = Value;
};

template<typename>
struct IsOpenRange : std::false_type {};
template<bool MaxBound, size_t Value>
struct IsOpenRange<OpenRange<MaxBound, Value>> : std::true_type {};

template<size_t Value>
struct ExactValue {
  static constexpr size_t VALUE = Value;
};

template<typename>
struct IsExactValue : std::false_type {};
template<size_t Value>
struct IsExactValue<ExactValue<Value>> : std::true_type {};

[[maybe_unused]] inline Result notImplementedEE(char const* funcName) {
  return {TRI_ERROR_NOT_IMPLEMENTED,
          absl::StrCat("Function '", funcName,
                       "' is available in ArangoDB Enterprise Edition only.")};
}

template<typename RangeType>
Result invalidArgsCount(char const* funcName) {
  if constexpr (IsRange<RangeType>::value) {
    return {TRI_ERROR_BAD_PARAMETER,
            "'"s.append(funcName)
                .append("' AQL function: Invalid number of arguments passed "
                        "(expected >= ")
                .append(std::to_string(RangeType::MIN))
                .append(" and <= ")
                .append(std::to_string(RangeType::MAX))
                .append(")")};
  } else if constexpr (IsOpenRange<RangeType>::value) {
    if constexpr (RangeType::MAX_BOUND) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'"s.append(funcName)
                  .append("' AQL function: Invalid number of arguments passed "
                          "(expected <= ")
                  .append(std::to_string(RangeType::VALUE))
                  .append(")")};
    }

    return {TRI_ERROR_BAD_PARAMETER,
            "'"s.append(funcName)
                .append("' AQL function: Invalid number of arguments passed "
                        "(expected >= ")
                .append(std::to_string(RangeType::VALUE))
                .append(")")};
  } else if constexpr (IsExactValue<RangeType>::value) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName)
            .append(
                "' AQL function: Invalid number of arguments passed (expected ")
            .append(std::to_string(RangeType::VALUE))
            .append(")")};
  }

  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName).append(
              "' AQL function: Invalid number of arguments passed")};
}

inline Result negativeNumber(char const* funcName, size_t i) {
  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName)
              .append("' AQL function: argument at position '")
              .append(std::to_string(i))
              .append("' must be a positive number")};
}

inline Result nondeterministicArgs(char const* funcName) {
  return {TRI_ERROR_BAD_PARAMETER,
          "Unable to handle non-deterministic arguments for '"s.append(funcName)
              .append("' function")};
}

inline Result nondeterministicArg(char const* funcName, size_t i) {
  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName)
              .append("' AQL function: argument at position '")
              .append(std::to_string(i))
              .append("' is intended to be deterministic")};
}

inline Result invalidAttribute(char const* funcName, size_t i) {
  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName)
              .append("' AQL function: Unable to parse argument at position '")
              .append(std::to_string(i))
              .append("' as an attribute identifier")};
}

inline Result invalidArgument(char const* funcName, size_t i) {
  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName)
              .append("' AQL function: argument at position '")
              .append(std::to_string(i))
              .append("' is invalid")};
}

inline Result failedToEvaluate(const char* funcName, size_t i) {
  return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName)
          .append("' AQL function: Failed to evaluate argument at position '")
          .append(std::to_string(i))
          .append(("'"))};
}

inline Result typeMismatch(const char* funcName, size_t i,
                           ScopedValueType expectedType,
                           ScopedValueType actualType) {
  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName)
              .append("' AQL function: argument at position '")
              .append(std::to_string(i))
              .append("' has invalid type '")
              .append(ScopedAqlValue::typeString(actualType).data())
              .append("' ('")
              .append(ScopedAqlValue::typeString(expectedType).data())
              .append("' expected)")};
}

inline Result failedToParse(char const* funcName, size_t i,
                            ScopedValueType expectedType) {
  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName)
              .append("' AQL function: Unable to parse argument at position '")
              .append(std::to_string(i))
              .append("' as ")
              .append(ScopedAqlValue::typeString(expectedType).data())};
}

inline Result failedToGenerateName(char const* funcName, size_t i) {
  return {TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName)
              .append("' AQL function: Failed to generate field name from the "
                      "argument at position '")
              .append(std::to_string(i))
              .append("'")};
}

inline Result malformedNode(aql::AstNodeType type) {
  auto const* typeName = getNodeTypeName(type);

  std::string message("Can't process malformed AstNode of type '");
  if (typeName) {
    message += *typeName;
  } else {
    message += std::to_string(type);
  }
  message += "'";

  return {TRI_ERROR_BAD_PARAMETER, message};
}

}  // namespace error

template<typename T, bool CheckDeterminism = false>
Result evaluateArg(T& out, ScopedAqlValue& value, char const* funcName,
                   aql::AstNode const& args, size_t i, bool isFilter,
                   QueryContext const& ctx) {
  static_assert(std::is_same<T, std::string_view>::value ||
                std::is_same<T, int64_t>::value ||
                std::is_same<T, double_t>::value ||
                std::is_same<T, bool>::value);

  auto const* arg = args.getMemberUnchecked(i);

  if (!arg) {
    return error::invalidArgument(funcName, 2);
  }

  if constexpr (CheckDeterminism) {
    if (!arg->isDeterministic()) {
      return error::nondeterministicArg(funcName, i);
    }
  }

  value.reset(*arg);

  if (isFilter || value.isConstant()) {
    if (!value.execute(ctx)) {
      return error::failedToEvaluate(funcName, i + 1);
    }

    ScopedValueType expectedType = ScopedValueType::SCOPED_VALUE_TYPE_INVALID;
    if constexpr (std::is_same<T, std::string_view>::value) {
      expectedType = SCOPED_VALUE_TYPE_STRING;
    } else if constexpr (std::is_same<T, int64_t>::value ||
                         std::is_same<T, double_t>::value) {
      expectedType = SCOPED_VALUE_TYPE_DOUBLE;
    } else if constexpr (std::is_same<T, bool>::value) {
      expectedType = SCOPED_VALUE_TYPE_BOOL;
    }

    if (expectedType != value.type()) {
      return error::typeMismatch(funcName, i + 1, expectedType, value.type());
    }

    if constexpr (std::is_same<T, std::string_view>::value) {
      if (!value.getString(out)) {
        return error::failedToParse(funcName, i + 1, expectedType);
      }
    } else if constexpr (std::is_same<T, int64_t>::value) {
      out = value.getInt64();
    } else if constexpr (std::is_same<T, double>::value) {
      if (!value.getDouble(out)) {
        return error::failedToParse(funcName, i + 1, expectedType);
      }
    } else if constexpr (std::is_same<T, bool>::value) {
      out = value.getBoolean();
    }
  }

  return {};
}

inline Result getAnalyzerByName(FieldMeta::Analyzer& out,
                                const std::string_view& analyzerId,
                                char const* funcName, QueryContext const& ctx) {
  TRI_ASSERT(ctx.trx);
  auto& server = ctx.trx->vocbase().server();
  if (!server.hasFeature<IResearchAnalyzerFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "'"s.append(IResearchAnalyzerFeature::name())
                .append("' feature is not registered, unable to evaluate '")
                .append(funcName)
                .append("' function")};
  }

  auto& analyzerFeature = server.getFeature<IResearchAnalyzerFeature>();
  auto& [analyzer, shortName] = out;

  analyzer = analyzerFeature.get(analyzerId, ctx.trx->vocbase(),
                                 ctx.trx->state()->analyzersRevision());
  if (!analyzer) {
    return {TRI_ERROR_BAD_PARAMETER,
            "'"s.append("' AQL function: Unable to load requested analyzer '")
                .append(analyzerId.data(), analyzerId.size())
                .append("'")};
  }

  shortName = IResearchAnalyzerFeature::normalize(
      analyzerId, ctx.trx->vocbase().name(), false);

  return {};
}

inline Result extractAnalyzerFromArg(FieldMeta::Analyzer& out,
                                     char const* funcName,
                                     irs::boolean_filter const* filter,
                                     aql::AstNode const& args, size_t i,
                                     QueryContext const& ctx) {
  auto const* analyzerArg = args.getMemberUnchecked(i);

  if (!analyzerArg) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName, "' AQL function: ", i + 1,
                         " argument is invalid analyzer")};
  }

  ScopedAqlValue analyzerValue(*analyzerArg);
  std::string_view analyzerId;

  auto rv =
      evaluateArg(analyzerId, analyzerValue, funcName, args, i, filter, ctx);

  if (rv.fail()) {
    return rv;
  }

  if (!filter && !analyzerValue.isConstant()) {
    return {};
  }

  return getAnalyzerByName(out, analyzerId, funcName, ctx);
}

}  // namespace arangodb::iresearch
