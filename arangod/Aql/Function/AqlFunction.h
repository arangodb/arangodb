////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlValue.h"
#include "Aql/Function/FunctionContext.h"
#include "Aql/Functions.h"
#include "Basics/ResultT.h"

#include <concepts>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace arangodb::aql {
struct AstNode;
class ExpressionContext;

namespace functions {
namespace detail {

template<typename F, typename Tuple>
constexpr bool kIsRunnableWith = false;

// Can AQL function with with tuple arguments?
// This checks all overloads
template<typename F, typename... Args>
constexpr bool kIsRunnableWith<F, std::tuple<Args...>> =
    requires(FunctionContext const& ctx, Args&&... args) {
  {
    F::run(ctx, std::forward<Args>(args)...)
    } -> std::same_as<ResultT<AqlValue>>;
};

template<typename F, typename Variant>
constexpr bool kIsRunnableWithAll = false;

// All overloads must be satisfied
template<typename F, typename... Alternatives>
constexpr bool kIsRunnableWithAll<F, std::variant<Alternatives...>> =
    (kIsRunnableWith<F, Alternatives> && ...);

auto toAqlValue(ExpressionContext& expressionContext,
                std::string_view functionName, ResultT<AqlValue> result)
    -> AqlValue;

}  // namespace detail

// parseArguments() picks an F::Arguments alternative, run() handles each one
template<typename F>
concept AqlFunction = requires(FunctionContext const& ctx,
                               std::span<AqlValue const> args) {
  { F::kName } -> std::convertible_to<std::string_view>;
  { F::kSignature } -> std::convertible_to<char const*>;
  {
    F::parseArguments(ctx, args)
    } -> std::same_as<ResultT<typename F::Arguments>>;
}
&&detail::kIsRunnableWithAll<F, typename F::Arguments>;

template<AqlFunction F>
auto invoke(FunctionContext const& ctx, std::span<AqlValue const> args)
    -> ResultT<AqlValue> {
  auto parsed = F::parseArguments(ctx, args);
  if (parsed.fail()) {
    return std::move(parsed).result();
  }
  return std::visit(
      [&](auto&& arguments) {
        return std::apply(
            [&](auto&&... values) {
              return F::run(ctx, std::forward<decltype(values)>(values)...);
            },
            std::forward<decltype(arguments)>(arguments));
      },
      std::move(*parsed));
}

template<AqlFunction F>
auto adapt(ExpressionContext* expressionContext, AstNode const&,
           VPackFunctionParametersView parameters) -> AqlValue {
  auto const ctx = FunctionContext::fromExpressionContext(*expressionContext);
  return detail::toAqlValue(*expressionContext, F::kName,
                            invoke<F>(ctx, parameters));
}

}  // namespace functions
}  // namespace arangodb::aql
