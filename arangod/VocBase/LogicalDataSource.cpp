////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "LogicalDataSource.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "RestServer/ServerIdFeature.h"
#include "Utilities/NameValidator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include "Logger/Logger.h"

namespace arangodb {
namespace {

std::string ensureGuid(std::string&& guid, DataSourceId id, DataSourceId planId,
                       std::string const& name, bool isSystem) {
  if (!guid.empty()) {
    return std::move(guid);
  }
  guid.reserve(64);
  // view GUIDs are added to the ClusterInfo. To avoid conflicts
  // with collection names we always put in a '/' which is an illegal
  // character for collection names. Stringified collection or view
  // id numbers can also not conflict, first character is always 'h'
  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isDBServer()) {
    // ensured by LogicalDataSource constructor + '_id' != 0
    TRI_ASSERT(planId);
    guid.append("c");
    guid.append(std::to_string(planId.id()));
    guid.push_back('/');
    if (ServerState::instance()->isDBServer()) {
      // we add the shard name to the collection. If we ever
      // replicate shards, we can identify them cluster-wide
      guid.append(name);
    }
  } else if (isSystem) {
    guid.append(name);
  } else {
    TRI_ASSERT(id);  // ensured by ensureId(...)
    char buf[sizeof(ServerId) * 2 + 1];
    auto len = TRI_StringUInt64HexInPlace(ServerIdFeature::getId().id(), buf);
    guid.append("h");
    guid.append(buf, len);
    TRI_ASSERT(guid.size() > 3);
    guid.push_back('/');
    guid.append(std::to_string(id.id()));
  }
  return std::move(guid);
}

DataSourceId ensureId(TRI_vocbase_t& vocbase, DataSourceId id) {
  if (id) {
    return id;
  }
  if (!ServerState::instance()->isCoordinator() &&
      !ServerState::instance()->isDBServer()) {
    return DataSourceId(TRI_NewTickServer());
  }
  auto& server = vocbase.server();
  TRI_ASSERT(server.hasFeature<ClusterFeature>());
  auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
  id = DataSourceId{ci.uniqid(1)};
  if (!id) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "invalid zero value returned for uniqueid by 'ClusterInfo' while "
        "generating LogicalDataSource ID");
  }
  return id;
}

bool readIsSystem(velocypack::Slice definition) {
  if (!definition.isObject()) {
    return false;
  }
  auto name = basics::VelocyPackHelper::getStringValue(
      definition, StaticStrings::DataSourceName, StaticStrings::Empty);
  if (!NameValidator::isSystemName(name)) {
    return false;
  }
  // same condition as in LogicalCollection
  return basics::VelocyPackHelper::getBooleanValue(
      definition, StaticStrings::DataSourceSystem, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create an velocypack::ValuePair for a string value
//////////////////////////////////////////////////////////////////////////////
velocypack::ValuePair toValuePair(std::string const& value) {
  return {&value[0], value.size(), velocypack::ValueType::String};
}

}  // namespace

static_assert(LogicalCollection::category() ==
              LogicalDataSource::Category::kCollection);
static_assert(LogicalView::category() == LogicalDataSource::Category::kView);

LogicalDataSource::LogicalDataSource(Category category, TRI_vocbase_t& vocbase,
                                     velocypack::Slice definition)
    : LogicalDataSource(
          category, vocbase,
          DataSourceId{basics::VelocyPackHelper::extractIdValue(definition)},
          basics::VelocyPackHelper::getStringValue(
              definition, StaticStrings::DataSourceGuid, ""),
          DataSourceId{basics::VelocyPackHelper::stringUInt64(
              definition.get(StaticStrings::DataSourcePlanId))},
          basics::VelocyPackHelper::getStringValue(
              definition, StaticStrings::DataSourceName, ""),
          readIsSystem(definition),
          basics::VelocyPackHelper::getBooleanValue(
              definition, StaticStrings::DataSourceDeleted, false)) {}

LogicalDataSource::LogicalDataSource(Category category, TRI_vocbase_t& vocbase,
                                     DataSourceId id, std::string&& guid,
                                     DataSourceId planId, std::string&& name,
                                     bool system, bool deleted)
    : _name(std::move(name)),
      _vocbase(vocbase),
      _id(ensureId(vocbase, id)),
      _planId(planId ? planId : _id),
      _guid(ensureGuid(std::move(guid), _id, _planId, _name, system)),
      _deleted(deleted),
      _category(category),
      _system(system) {
  TRI_ASSERT(_id);
  TRI_ASSERT(!_guid.empty());
}

Result LogicalDataSource::properties(velocypack::Builder& build,
                                     Serialization ctx, bool safe) const {
  if (!build.isOpenObject()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "invalid builder provided for data-source definition"};
  }
  // required for dump/restore
  build.add(StaticStrings::DataSourceGuid, toValuePair(guid()));

  build.add(StaticStrings::DataSourceId,
            velocypack::Value(std::to_string(id().id())));
  build.add(StaticStrings::DataSourceName, toValuePair(name()));
  // note: includeSystem and forPersistence are not 100% synonymous,
  // however, for our purposes this is an okay mapping;
  // we only set includeSystem if we are persisting the properties
  if (ctx == Serialization::Persistence ||
      ctx == Serialization::PersistenceWithInProgress) {
    build.add(StaticStrings::DataSourceDeleted, velocypack::Value(deleted()));
    build.add(StaticStrings::DataSourceSystem, velocypack::Value(system()));
    // TODO not sure if the following is relevant
    build.add(StaticStrings::DataSourcePlanId,  // Cluster Specific
              velocypack::Value(std::to_string(planId().id())));
  }
  return appendVPack(build, ctx, safe);
}

}  // namespace arangodb
