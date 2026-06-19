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

#include "Inspection/Status.h"

namespace arangodb::vector {

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

enum class FilterMode : std::uint8_t {
  kNone,
  kStoredValues,
  kDocument,
};

template<class Inspector>
inline auto inspect(Inspector& f, FilterMode& x) {
  return f.enumeration(x).values(FilterMode::kNone, "none",
                                 FilterMode::kStoredValues, "storedValues",
                                 FilterMode::kDocument, "document");
}

enum class ProjectionMode : std::uint8_t {
  kPassThroughId,
  kCovered,
  kDocument,
};

template<class Inspector>
inline auto inspect(Inspector& f, ProjectionMode& x) {
  return f.enumeration(x).values(ProjectionMode::kPassThroughId,
                                 "pass-through-id", ProjectionMode::kCovered,
                                 "covering", ProjectionMode::kDocument,
                                 "document");
}

struct SearchStrategy {
  FilterMode filter{FilterMode::kNone};
  ProjectionMode projection{ProjectionMode::kPassThroughId};
};

}  // namespace arangodb::vector
