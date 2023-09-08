////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchFilterOptimization.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchInvertedIndexMeta.h"

#include "VocBase/voc-types.h"

#include "search/filter.hpp"

#include <function2.hpp>

namespace arangodb {
class Result;

namespace aql {

struct AstNode;
class ExpressionContext;

}  // namespace aql

namespace iresearch {

struct QueryContext;
struct FilterContext;

struct FilterFactory {
  // Determine if the 'node' can be converted into an iresearch filter
  // if 'filter' != nullptr then also append the iresearch filter there
  static Result filter(irs::boolean_filter* filter,
                       FilterContext const& filterCtx,
                       aql::AstNode const& node);
};

struct FilterConstants {
  // Defaults
  static constexpr size_t DefaultScoringTermsLimit{128};
  static constexpr size_t DefaultLevenshteinTermsLimit{64};
  static constexpr double DefaultNgramMatchThreshold{0.7};
  static constexpr int64_t DefaultStartsWithMinMatchCount{1};
};

}  // namespace iresearch
}  // namespace arangodb
