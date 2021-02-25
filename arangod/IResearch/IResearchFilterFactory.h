////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_FILTER_FACTORY_H
#define ARANGOD_IRESEARCH__IRESEARCH_FILTER_FACTORY_H 1

#include "VocBase/voc-types.h"

#include "search/filter.hpp"

namespace iresearch {

class boolean_filter;  // forward declaration

}  // namespace iresearch

namespace arangodb {
class Result;

namespace aql {

struct AstNode;  // forward declaration

}  // namespace aql

namespace iresearch {

struct QueryContext;

struct FilterFactory {
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch filter
  ///        if 'filter' != nullptr then also append the iresearch filter there
  ////////////////////////////////////////////////////////////////////////////////
  static arangodb::Result filter(irs::boolean_filter* filter, QueryContext const& ctx,
                     arangodb::aql::AstNode const& node);
};  // FilterFactory


struct FilterConstants {
  // Defaults
  static constexpr size_t DefaultScoringTermsLimit { 128 };
  static constexpr size_t DefaultLevenshteinTermsLimit { 64 };
  static constexpr double_t DefaultNgramMatchThreshold { 0.7 };
  static constexpr int64_t DefaultStartsWithMinMatchCount { 1 };
};

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_FILTER_FACTORY_H
