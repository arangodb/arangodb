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

#include "Basics/StaticStrings.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Properties/InspectContexts.h"

#include <functional>
#include <string>

namespace arangodb {

struct DatabaseConfiguration;
class Result;

namespace inspection {
struct Status;
}

/// The identifiers a collection is known by. All of them are assigned by the
/// server; a client never picks one.
struct CollectionIdentity {
  struct Transformers {
    struct IdIdentifier {
      using MemoryType = DataSourceId;
      using SerializedType = std::string;

      static arangodb::inspection::Status toSerialized(MemoryType v,
                                                       SerializedType& result);

      static arangodb::inspection::Status fromSerialized(
          SerializedType const& v, MemoryType& result);
    };
  };

  DataSourceId id{0};
  // Read back on load. Empty means the collection does not have one yet.
  std::string guid = StaticStrings::Empty;

  [[nodiscard]] arangodb::Result applyDefaultsAndValidateDatabaseConfiguration(
      DatabaseConfiguration const& config);

  bool operator==(CollectionIdentity const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionIdentity& props) {
  auto idField = std::invoke([&]() {
    if constexpr (isInternalContext<Inspector>) {
      // Markers and plan entries store the id as a number or under "cid".
      // LogicalDataSource owns the id on that path, so skip it here.
      return f.ignoreField(StaticStrings::Id);
    } else {
      return f.field(StaticStrings::Id, props.id)
          .transformWith(CollectionIdentity::Transformers::IdIdentifier{})
          .fallback(f.keep());
    }
  });

  return f.object(props).fields(
      std::move(idField),
      internalOnlyField(f, StaticStrings::DataSourceGuid, props.guid));
}

}  // namespace arangodb
