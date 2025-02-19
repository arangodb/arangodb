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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchAnalyzerFeature.h"
#include "search/phrase_filter.hpp"
#include "utils/string.hpp"

#include <unicode/regex.h>

#include <vector>
#include <string>
#include <cstddef>

namespace arangodb::aql {

class ExpressionContext;

}  // namespace arangodb::aql
namespace arangodb::iresearch::wildcard {

class Filter;

struct Options {
  using filter_type = Filter;

  std::vector<irs::by_phrase_options> parts;
  irs::bstring token;
  bool hasPos{true};
  icu_64_64::RegexMatcher* matcher{};

  // TODO(MBkkt) implement this when we will make filter caching
  bool operator==(const Options& rhs) const noexcept { return false; }

  Options() noexcept = default;
  Options(Options&&) noexcept = default;
  Options& operator=(Options&&) noexcept = default;

  Options(std::string_view pattern, AnalyzerPool const& analyzer,
          aql::ExpressionContext* ctx);
};

}  // namespace arangodb::iresearch::wildcard
