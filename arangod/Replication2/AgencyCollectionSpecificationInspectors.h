////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/StaticStrings.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "VocBase/Properties/UtilityInvariants.h"

namespace arangodb::replication2::agency {

template<class Inspector>
auto inspect(Inspector& f, CollectionGroup::Attributes::MutableAttributes& x) {
  return f.object(x).fields(
      f.field(StaticStrings::WriteConcern, x.writeConcern),
      f.field(StaticStrings::ReplicationFactor, x.replicationFactor),
      f.field(StaticStrings::WaitForSyncString, x.waitForSync));
}

template<class Inspector>
auto inspect(Inspector& f,
             CollectionGroup::Attributes::ImmutableAttributes& x) {
  return f.object(x).fields(
      f.field(StaticStrings::NumberOfShards, x.numberOfShards));
}

template<class Inspector>
auto inspect(Inspector& f, CollectionGroup::Attributes& x) {
  return f.object(x).fields(f.field("mutable", x.mutableAttributes),
                            f.field("immutable", x.immutableAttributes));
}

template<class Inspector>
auto inspect(Inspector& f, CollectionGroup& x) {
  return f.object(x).fields(f.field(StaticStrings::Id, x.id),
                            f.field("collections", x.collections),
                            f.field("attributes", x.attributes));
}

template<class Inspector>
auto inspect(Inspector& f, CollectionGroup::Collection& x) {
  return f.object(x).fields();
}

template<class Inspector>
auto inspect(Inspector& f, CollectionGroupTargetSpecification& x) {
  return f.object(x).fields(f.template embedFields<CollectionGroup>(x));
}

template<class Inspector>
auto inspect(Inspector& f, CollectionGroupPlanSpecification::ShardSheaf& x) {
  return f.object(x).fields(f.field("replicatedLog", x.replicatedLog));
}

template<class Inspector>
auto inspect(Inspector& f, CollectionGroupPlanSpecification& x) {
  return f.object(x).fields(f.template embedFields<CollectionGroup>(x),
                            f.field("shardSheaves", x.shardSheaves));
}

template<class Inspector>
auto inspect(Inspector& f, Collection::MutableProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::Schema, props.schema),
      f.field(StaticStrings::ComputedValues, props.computedValues));
}

template<class Inspector>
auto inspect(Inspector& f, Collection::ImmutableProperties& props) {
  return f.object(props).fields(
      f.field(StaticStrings::DataSourceName, props.name)
          .invariant(UtilityInvariants::isNonEmpty),
      f.field(StaticStrings::DataSourceSystem, props.isSystem),
      f.field(StaticStrings::IsSmart, props.isSmart),
      f.field(StaticStrings::IsDisjoint, props.isDisjoint),
      f.field(StaticStrings::CacheEnabled, props.cacheEnabled),
      f.field(StaticStrings::GraphSmartGraphAttribute,
              props.smartGraphAttribute)
          .invariant(UtilityInvariants::isNonEmptyIfPresent),
      f.field(StaticStrings::SmartJoinAttribute, props.smartJoinAttribute)
          .invariant(UtilityInvariants::isNonEmptyIfPresent),
      f.field(StaticStrings::DataSourceType, props.type),
      f.field(StaticStrings::KeyOptions, props.keyOptions),
      f.field(StaticStrings::ShadowCollections, props.shadowCollections),
      f.template embedFields<CollectionInternalProperties>(props));
}

template<class Inspector>
auto inspect(Inspector& f, Collection& x) {
  return f.object(x).fields(
      f.field("groupId", x.groupId),
      f.field("mutableProperties", x.mutableProperties),
      f.field("immutableProperties", x.immutableProperties));
}

template<class Inspector>
auto inspect(Inspector& f, CollectionTargetSpecification& x) {
  return f.object(x).fields(f.template embedFields<Collection>(x));
}

template<class Inspector>
auto inspect(Inspector& f, CollectionPlanSpecification& x) {
  return f.object(x).fields(
      f.template embedFields<Collection>(x),
      /* NOTE: shardsR2 is a temporary key. We plan to replace it by shards
         before release, which right now is occupied */
      f.field("shardsR2", x.shardList),
      f.field("shards", x.deprecatedShardMap));
}

template<class Inspector>
auto inspect(Inspector& f, ClusterPhysicalCollectionSpec& x) {
  return f.object(x).fields(
      f.field(StaticStrings::CacheEnabled, x.cacheEnabled)
      );
}

template<class Inspector>
auto inspect(Inspector& f, RocksDBPhysicalCollectionSpec& x) {
  return f.object(x).fields(
      f.field(StaticStrings::ObjectId, x.objectId)
      );
}

template<class Inspector>
auto inspect(Inspector& f, PhysicalCollectionSpec& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).unqualified().alternatives(
      insp::inlineType<RocksDBPhysicalCollectionSpec>(),
      insp::inlineType<ClusterPhysicalCollectionSpec>());
}

}  // namespace arangodb::replication2::agency
