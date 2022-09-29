////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

// TODO Remove me, just used while the inspection is not complete
#include "Basics/Exceptions.h"

#include "Basics/StaticStrings.h"
#include "Inspection/Status.h"
#include "Inspection/VPackLoadInspector.h"
#include "VocBase/Properties/CollectionConstantProperties.h"
#include "VocBase/Properties/CollectionCreateOptions.h"
#include "VocBase/Properties/CollectionMutableProperties.h"
#include "VocBase/Properties/CollectionInternalProperties.h"
#include "VocBase/Properties/UtilityInvariants.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

#include <string>

namespace arangodb {

class DataSourceId;
class Result;

template<typename T>
class ResultT;

namespace inspection {
struct Status;
}
namespace velocypack {
class Slice;
}

struct PlanCollection {
  struct DatabaseConfiguration {
#if ARANGODB_USE_GOOGLE_TESTS
    // Default constructor for testability.
    // In production, we need to use vocbase
    // constructor.
    DatabaseConfiguration(std::function<DataSourceId()> idGenerator);
#endif
    explicit DatabaseConfiguration(TRI_vocbase_t const& database);

    bool allowExtendedNames = false;
    bool shouldValidateClusterSettings = false;
    uint32_t maxNumberOfShards = 0;

    uint32_t minReplicationFactor = 0;
    uint32_t maxReplicationFactor = 0;
    bool enforceReplicationFactor = true;

    uint64_t defaultNumberOfShards = 1;
    uint64_t defaultReplicationFactor = 1;
    uint64_t defaultWriteConcern = 1;
    std::string defaultDistributeShardsLike = "";
    bool isOneShardDB = false;

    std::function<DataSourceId()> idGenerator;
  };

  PlanCollection();

  static ResultT<PlanCollection> fromCreateAPIBody(
      arangodb::velocypack::Slice input, DatabaseConfiguration config);

  static ResultT<PlanCollection> fromCreateAPIV8(
      arangodb::velocypack::Slice properties, std::string const& name,
      TRI_col_type_e type, DatabaseConfiguration config);

  static arangodb::velocypack::Builder toCreateCollectionProperties(
      std::vector<PlanCollection> const& collections);

  // Temporary method to handOver information from
  [[nodiscard]] arangodb::velocypack::Builder toCollectionsCreate() const;

  [[nodiscard]] arangodb::Result validateDatabaseConfiguration(
      DatabaseConfiguration config) const;

  CollectionConstantProperties constantProperties;
  CollectionMutableProperties mutableProperties;
  CollectionInternalProperties internalProperties;
  CollectionCreateOptions options;
};

template<class Inspector>
auto inspect(Inspector& f, PlanCollection& planCollection) {
  // TODO This is waiting on inspector with fields on same toplevel object
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return f.object(planCollection).fields();
}

}  // namespace arangodb
