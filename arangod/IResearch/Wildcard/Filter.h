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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/Wildcard/Options.h"

#include "search/filter.hpp"

namespace arangodb::aql {
class QueryContext;
}

namespace arangodb::iresearch::wildcard {

// Carries running query to wildcard iterator so it stops when killed
struct QueryKillCheck final : irs::attribute {
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::wildcard::QueryKillCheck";
  }
  aql::QueryContext const* query{nullptr};
};

class Filter final : public irs::FilterWithField<Options> {
 public:
  prepared::ptr prepare(const irs::PrepareContext& ctx) const final;
};

}  // namespace arangodb::iresearch::wildcard
