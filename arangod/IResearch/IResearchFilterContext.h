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

#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchFilterOptimization.h"

namespace arangodb {

namespace aql {
class Ast;
class ExpressionContext;
class Variable;
}  // namespace aql
namespace transaction {
class Methods;
}

namespace iresearch {

struct InvertedIndexField;

struct QueryContext {
  transaction::Methods* trx{};
  aql::Ast* ast{};
  aql::ExpressionContext* ctx{};
  ::iresearch::index_reader const* index{};
  aql::Variable const* ref{};
  // Allow optimize away/modify some conditions during filter building
  FilterOptimization filterOptimization{FilterOptimization::MAX};
  std::span<const InvertedIndexField> fields{};
  // The flag is set when a query is dedicated to a search view
  bool isSearchQuery{true};
  bool isOldMangling{true};
  bool hasNestedFields{false};
};

using AnalyzerProvider = fu2::unique_function<FieldMeta::Analyzer const&(
    std::string_view, aql::ExpressionContext*, FieldMeta::Analyzer const&)>;

struct FilterContext {
  FieldMeta::Analyzer const& fieldAnalyzer(
      std::string_view name, aql::ExpressionContext* ctx) const noexcept {
    return fieldAnalyzerProvider
               ? (*fieldAnalyzerProvider)(name, ctx, contextAnalyzer)
               : contextAnalyzer;  // Only possible with ArangoSearch view
  }

  QueryContext const& query;
  // need shared_ptr since pool could be deleted from the feature
  FieldMeta::Analyzer const& contextAnalyzer;
  AnalyzerProvider* fieldAnalyzerProvider{};
  std::string_view namePrefix{};  // field name prefix
  irs::score_t boost{irs::kNoBoost};
};

}  // namespace iresearch
}  // namespace arangodb
