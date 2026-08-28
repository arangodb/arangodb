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
#include "Inspection/Types.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Properties/InspectContexts.h"

#include <string>

namespace arangodb {

struct DatabaseConfiguration;
class Result;

namespace inspection {
struct Status;
}

namespace velocypack {
class Builder;
}

/// The identifiers a collection is known by. All of them are assigned by the
/// server; a client never picks one.
struct CollectionIdentity {
  struct Transformers {
    /// Serialized form is a string. Markers and plan entries may store the id
    /// as a number instead, so the internal path accepts one, the way the
    /// slice path did through VelocyPackHelper::extractIdValue. User input
    /// must spell it as a string.
    struct IdIdentifier {
      using MemoryType = DataSourceId;
      using SerializedType = arangodb::velocypack::Builder;

      bool acceptNumber{false};

      arangodb::inspection::Status toSerialized(MemoryType v,
                                                SerializedType& result) const;

      arangodb::inspection::Status fromSerialized(SerializedType const& v,
                                                  MemoryType& result) const;
    };
  };

  DataSourceId id{0};
  // Read back on load. Empty means the collection does not have one yet.
  std::string guid = StaticStrings::Empty;
  // The cluster-wide id of the collection a shard belongs to. Zero means the
  // collection is its own plan entry, which LogicalDataSource resolves to `id`.
  DataSourceId planId{DataSourceId::none()};

  [[nodiscard]] arangodb::Result applyDefaultsAndValidateDatabaseConfiguration(
      DatabaseConfiguration const& config);

  bool operator==(CollectionIdentity const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionIdentity& props) {
  // cid and planId are server-owned and were never declared on the user path,
  // so Reject reproduces the unexpected-attribute error the create API gave.
  auto serverOwned = []() {
    return isInternalContext<Inspector> ? inspection::FieldCondition::Process
                                        : inspection::FieldCondition::Reject;
  };

  // Only markers and plan entries may spell an id as a number
  constexpr auto idTransformer = CollectionIdentity::Transformers::IdIdentifier{
      .acceptNumber = isInternalContext<Inspector>};

  return f.object(props).fields(
      // declared before "id" so that "id" wins when a pre-3.1 collection
      // carries both
      f.field(StaticStrings::DataSourceCid, props.id)
          .transformWith(idTransformer)
          .fallback(f.keep())
          .when(serverOwned),
      f.field(StaticStrings::Id, props.id)
          .transformWith(idTransformer)
          .fallback(f.keep()),
      // guid is documented as having no effect, so on the user path it stays
      // accepted and is dropped
      f.field(StaticStrings::DataSourceGuid, props.guid)
          .fallback(f.keep())
          .when([]() {
            return isInternalContext<Inspector>
                       ? inspection::FieldCondition::Process
                       : inspection::FieldCondition::Ignore;
          }),
      f.field(StaticStrings::DataSourcePlanId, props.planId)
          .transformWith(idTransformer)
          .fallback(f.keep())
          .when(serverOwned));
}

}  // namespace arangodb
