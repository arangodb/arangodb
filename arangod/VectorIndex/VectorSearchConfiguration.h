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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Inspection/Status.h"

namespace arangodb::vector {

// Per-query knobs for a vector index search. Carried on the query plan and
// passed to the storage layer at execution time.
struct SearchParameters {
  std::optional<std::int64_t> nProbe;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, SearchParameters& x) {
    return f.object(x).fields(
        f.field("nProbe", x.nProbe)
            .invariant([](auto value) -> inspection::Status {
              if (value.has_value() && *value < 1) {
                return {"nProbe must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

// What the search must produce, from the caller's point of view. The
// storage layer maps this to a concrete iterator + capture shape (see the
// table in RocksDBVectorIndexList.h); callers do not name iterator types.
struct SearchStrategy {
  // How the filter, if any, gets evaluated by the iterator.
  enum class FilterMode : std::uint8_t {
    kNone,          // no filter pushed down
    kStoredValues,  // filter expressible against storedValues only
    kDocument,      // filter requires the full document
  };

  // What the executor needs, per surviving entry, to satisfy projections.
  enum class ProjectionSource : std::uint8_t {
    kNone,          // no projections (executor only emits labels/distances)
    kStoredValues,  // serve from a captured storedValues array
    kDocument,      // serve from a captured (or post-fetched) full document
  };

  FilterMode filter{FilterMode::kNone};
  ProjectionSource projection{ProjectionSource::kNone};
};

inline std::string_view filterModeName(
    SearchStrategy::FilterMode m) noexcept {
  switch (m) {
    case SearchStrategy::FilterMode::kNone:
      return "none";
    case SearchStrategy::FilterMode::kStoredValues:
      return "storedValues";
    case SearchStrategy::FilterMode::kDocument:
      return "document";
  }
  return "none";
}

inline SearchStrategy::FilterMode parseFilterMode(
    std::string_view name) noexcept {
  if (name == "storedValues") {
    return SearchStrategy::FilterMode::kStoredValues;
  }
  if (name == "document") {
    return SearchStrategy::FilterMode::kDocument;
  }
  return SearchStrategy::FilterMode::kNone;
}

}  // namespace arangodb::vector
