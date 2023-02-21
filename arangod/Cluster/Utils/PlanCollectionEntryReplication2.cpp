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

#include "PlanCollectionEntryReplication2.h"
#include "Inspection/VPack.h"
#include "Replication2/AgencyCollectionSpecificationInspectors.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>

#include "Logger/LogMacros.h"

using namespace arangodb;

namespace {
auto transform(UserInputCollectionProperties col)
    -> replication2::agency::CollectionTargetSpecification {
  // TODO Maybe we can kind a better way than this for transformation.
  replication2::agency::CollectionTargetSpecification spec;
  spec.groupId = col.groupId.value();
  spec.mutableProperties = {std::move(col.computedValues),
                            std::move(col.schema)};
  spec.immutableProperties = {col,
                              col.name,
                              col.isSystem,
                              col.type,
                              col.keyOptions,
                              col.isSmart,
                              col.isDisjoint,
                              col.cacheEnabled,
                              col.shardKeys.value(),
                              col.smartJoinAttribute,
                              col.smartGraphAttribute,
                              col.shadowCollections};
  spec.indexes = CollectionIndexesProperties::defaultIndexesForCollectionType(
      col.getType());
  return spec;
}
}  // namespace

PlanCollectionEntryReplication2::PlanCollectionEntryReplication2(
    UserInputCollectionProperties col)
    : _properties{::transform(std::move(col))} {}

std::string PlanCollectionEntryReplication2::getCID() const {
  TRI_ASSERT(!_properties.immutableProperties.id.empty());
  return std::to_string(_properties.immutableProperties.id.id());
}

std::string const& PlanCollectionEntryReplication2::getName() const {
  TRI_ASSERT(!_properties.immutableProperties.name.empty());
  return {_properties.immutableProperties.name};
}
