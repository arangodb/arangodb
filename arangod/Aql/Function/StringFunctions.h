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
#include "Basics/ResultT.h"

#include <unicode/unistr.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace arangodb::aql::functions {

struct Substitute {
  static constexpr std::string_view kName = "SUBSTITUTE";
  static constexpr char const* kSignature = ".,.|.,.";

  using Patterns = std::vector<icu_64_64::UnicodeString>;

  struct ReplaceAllWith {
    icu_64_64::UnicodeString replacement;
  };

  using Arguments = std::variant<
      std::tuple<AqlValue const&, Patterns, Patterns, int64_t>,
      std::tuple<AqlValue const&, Patterns, ReplaceAllWith, int64_t>>;

  static auto parseArguments(FunctionContext const& ctx,
                             std::span<AqlValue const> args)
      -> ResultT<Arguments>;

  static auto run(FunctionContext const& ctx, AqlValue const& value,
                  Patterns const& search, Patterns const& replacements,
                  int64_t limit) -> ResultT<AqlValue>;
  static auto run(FunctionContext const& ctx, AqlValue const& value,
                  Patterns const& search, ReplaceAllWith const& replaceAllWith,
                  int64_t limit) -> ResultT<AqlValue>;
};

}  // namespace arangodb::aql::functions
