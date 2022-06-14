////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchFilterOptimization.h"
#include "IResearchLinkMeta.h"

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
struct InvertedIndexField;

using AnalyzerProvider =
    std::function<FieldMeta::Analyzer const&(std::string_view)>;

class FilterContext {
 public:
  FilterContext(FieldMeta::Analyzer const& analyzer, irs::score_t boost,
                AnalyzerProvider const* provider,
                std::string_view namePrefix) noexcept
      : _analyzerProvider{provider},
        _analyzer{analyzer},
        _nested{},
        _namePrefix{namePrefix},
        _boost{boost} {
    TRI_ASSERT(_analyzer._pool);
  }

  FilterContext(FilterContext const&) = default;
  FilterContext& operator=(FilterContext const&) = delete;

  irs::score_t boost() const noexcept { return _boost; }

  FieldMeta::Analyzer const& analyzer() const noexcept { return _analyzer; }

  FieldMeta::Analyzer const& fieldAnalyzer(std::string_view name) const {
    if (_analyzerProvider == nullptr) {
      return _analyzer;
    }
    return (*_analyzerProvider)(name);
  }

  AnalyzerProvider const* analyzerProvider() const noexcept {
    return _analyzerProvider;
  }

  FieldMeta::Analyzer const& contextAnalyzer() const noexcept {
    return _analyzer;
  }

  std::string_view namePrefix() const noexcept { return _namePrefix; }

  InvertedIndexField const* nested() const noexcept { return _nested; }

 private:
  AnalyzerProvider const* _analyzerProvider;
  // need shared_ptr since pool could be deleted from the feature
  FieldMeta::Analyzer const& _analyzer;
  InvertedIndexField const* _nested;
  std::string_view _namePrefix;  // field name prefix
  irs::score_t _boost;
};

struct FilterFactory {
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch filter
  ///        if 'filter' != nullptr then also append the iresearch filter there
  ////////////////////////////////////////////////////////////////////////////////
  static arangodb::Result filter(irs::boolean_filter* filter,
                                 QueryContext const& ctx,
                                 FilterContext const& filterCtx,
                                 arangodb::aql::AstNode const& node);
};  // FilterFactory

struct FilterConstants {
  // Defaults
  static constexpr size_t DefaultScoringTermsLimit{128};
  static constexpr size_t DefaultLevenshteinTermsLimit{64};
  static constexpr double_t DefaultNgramMatchThreshold{0.7};
  static constexpr int64_t DefaultStartsWithMinMatchCount{1};
};

}  // namespace iresearch
}  // namespace arangodb
