////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <Basics/StaticStrings.h>
#include <Basics/debugging.h>

#include "AgencyCollectionSpecification.h"

using namespace arangodb::replication2::agency;
using namespace arangodb::basics;

CollectionGroup::CollectionGroup(VPackSlice slice)
    : id(CollectionGroupId{
          slice.get(StaticStrings::Id).extract<CollectionGroupId::Identifier::BaseType>()}),
      attributes(slice.get("attributes")) {
  {
    auto cs = slice.get("collections");
    TRI_ASSERT(cs.isObject());
    collections.reserve(cs.length());
    for (auto const& [key, value] : VPackObjectIterator(cs)) {
      auto cid = key.extract<std::string>();
      collections.emplace(std::move(cid), Collection(value));
    }
  }

  {
    auto sss = slice.get("shardSheaves");
    TRI_ASSERT(sss.isArray());
    shardSheaves.reserve(sss.length());
    for (auto const& rs : VPackArrayIterator(sss)) {
      shardSheaves.emplace_back(rs);
    }
  }
}

void CollectionGroup::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Id, VPackValue(id.id()));
  builder.add(VPackValue("attributes"));
  attributes.toVelocyPack(builder);
  {
    VPackObjectBuilder cb(&builder, "collections");
    for (auto const& [cid, collection] : collections) {
      builder.add(VPackValue(cid));
      collection.toVelocyPack(builder);
    }
  }
  {
    VPackArrayBuilder sb(&builder, "shardSheaves");
    for (auto const& sheaf : shardSheaves) {
      sheaf.toVelocyPack(builder);
    }
  }
}

CollectionGroup::Collection::Collection(VPackSlice slice) {
  TRI_ASSERT(slice.isEmptyObject());
}

void CollectionGroup::Collection::toVelocyPack(VPackBuilder& builder) const {
  builder.add(VPackSlice::emptyObjectSlice());
}

CollectionGroup::ShardSheaf::ShardSheaf(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());
  replicatedLog = LogId{slice.get("replicatedLog").extract<uint64_t>()};
}

void CollectionGroup::ShardSheaf::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("replicatedLog", VPackValue(replicatedLog.id()));
}

CollectionGroup::Attributes::Attributes(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());
  waitForSync = slice.get(StaticStrings::WaitForSyncString).extract<bool>();
  writeConcern = slice.get(StaticStrings::WriteConcern).extract<std::size_t>();
}

void CollectionGroup::Attributes::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::WaitForSyncString, VPackValue(waitForSync));
  builder.add(StaticStrings::WriteConcern, VPackValue(writeConcern));
}
