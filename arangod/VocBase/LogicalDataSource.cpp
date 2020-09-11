////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <mutex>

#include "LogicalDataSource.h"

#include <velocypack/StringRef.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "RestServer/ServerIdFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include "Logger/Logger.h"

namespace {

std::string ensureGuid(std::string&& guid, arangodb::DataSourceId id,
                       arangodb::DataSourceId planId, std::string const& name,
                       bool isSystem) {
  if (!guid.empty()) {
    return std::move(guid);
  }

  guid.reserve(64);

  // view GUIDs are added to the ClusterInfo. To avoid conflicts
  // with collection names we always put in a '/' which is an illegal
  // character for collection names. Stringified collection or view
  // id numbers can also not conflict, first character is always 'h'
  if (arangodb::ServerState::instance()->isCoordinator() ||
      arangodb::ServerState::instance()->isDBServer()) {
    TRI_ASSERT(planId); // ensured by LogicalDataSource constructor + '_id' != 0
    guid.append("c");
    guid.append(std::to_string(planId.id()));
    guid.push_back('/');
    if (arangodb::ServerState::instance()->isDBServer()) {
      // we add the shard name to the collection. If we ever
      // replicate shards, we can identify them cluster-wide
      guid.append(name);
    }
  } else if (isSystem) {
    guid.append(name);
  } else {
    TRI_ASSERT(id); // ensured by ensureId(...)
    char buf[sizeof(arangodb::ServerId) * 2 + 1];
    auto len = TRI_StringUInt64HexInPlace(arangodb::ServerIdFeature::getId().id(), buf);
    guid.append("h");
    guid.append(buf, len);
    TRI_ASSERT(guid.size() > 3);
    guid.push_back('/');
    guid.append(std::to_string(id.id()));
  }

  return std::move(guid);
}

arangodb::DataSourceId ensureId(TRI_vocbase_t& vocbase, arangodb::DataSourceId id) {
  if (id) {
    return id;
  }

  if (!arangodb::ServerState::instance()->isCoordinator() // not coordinator
      && !arangodb::ServerState::instance()->isDBServer() // not db-server
     ) {
    return arangodb::DataSourceId(TRI_NewTickServer());
  }

  TRI_ASSERT(vocbase.server().hasFeature<arangodb::ClusterFeature>());
  arangodb::ClusterInfo* ci = &vocbase.server().getFeature<arangodb::ClusterFeature>().clusterInfo();

  TRI_ASSERT(ci != nullptr);
  id = arangodb::DataSourceId{ci->uniqid(1)};

  if (!id) {
    THROW_ARANGO_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_INTERNAL, // code
      "invalid zero value returned for uniqueid by 'ClusterInfo' while generating LogicalDataSource ID" // message
    );
  }

  return id;
}

bool readIsSystem(arangodb::velocypack::Slice definition) {
  if (!definition.isObject()) {
    return false;
  }

  static const std::string empty;
  auto name = arangodb::basics::VelocyPackHelper::getStringValue(
      definition, arangodb::StaticStrings::DataSourceName, empty);

  if (!TRI_vocbase_t::IsSystemName(name)) {
    return false;
  }

  // same condition as in LogicalCollection
  return arangodb::basics::VelocyPackHelper::getBooleanValue(
      definition, arangodb::StaticStrings::DataSourceSystem, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create an arangodb::velocypack::ValuePair for a string value
//////////////////////////////////////////////////////////////////////////////
arangodb::velocypack::ValuePair toValuePair(std::string const& value) {
  return arangodb::velocypack::ValuePair(&value[0], value.size(),
                                         arangodb::velocypack::ValueType::String);
}

}  // namespace

namespace arangodb {

/*static*/ LogicalDataSource::Type const& LogicalDataSource::Type::emplace(
    arangodb::velocypack::StringRef const& name) {
  struct Less {
    bool operator()(arangodb::velocypack::StringRef const& lhs,
                    arangodb::velocypack::StringRef const& rhs) const noexcept {
      return lhs.compare(rhs) < 0;
    }
  };
  static std::mutex mutex;
  static std::map<arangodb::velocypack::StringRef, LogicalDataSource::Type, Less> types;
  std::lock_guard<std::mutex> lock(mutex);
  auto itr = types.try_emplace(name, Type());
  if (itr.second && name.data()) {
    const_cast<std::string&>(itr.first->second._name) = name.toString();  // update '_name'
    const_cast<arangodb::velocypack::StringRef&>(itr.first->first) =
        itr.first->second.name();  // point key at value stored in '_name'
  }

  return itr.first->second;
}

LogicalDataSource::LogicalDataSource(Category const& category, Type const& type,
                                     TRI_vocbase_t& vocbase,
                                     velocypack::Slice const& definition)
    : LogicalDataSource(
          category, type, vocbase,
          DataSourceId{basics::VelocyPackHelper::extractIdValue(definition)},
          basics::VelocyPackHelper::getStringValue(definition, StaticStrings::DataSourceGuid,
                                                   ""),
          DataSourceId{basics::VelocyPackHelper::stringUInt64(
              definition.get(StaticStrings::DataSourcePlanId))},
          basics::VelocyPackHelper::getStringValue(definition, StaticStrings::DataSourceName,
                                                   ""),
          readIsSystem(definition),
          basics::VelocyPackHelper::getBooleanValue(definition, StaticStrings::DataSourceDeleted,
                                                    false)) {}

LogicalDataSource::LogicalDataSource(Category const& category, Type const& type,
                                     TRI_vocbase_t& vocbase, DataSourceId id,
                                     std::string&& guid, DataSourceId planId,
                                     std::string&& name,
                                     bool system, bool deleted)
    : _name(std::move(name)),
      _category(category),
      _type(type),
      _vocbase(vocbase),
      _id(ensureId(vocbase, id)),
      _planId(planId ? planId : _id),
      _guid(ensureGuid(std::move(guid), _id, _planId, _name, system)),
      _deleted(deleted),
      _system(system) {
  TRI_ASSERT(_id);
  TRI_ASSERT(!_guid.empty());
}

Result LogicalDataSource::properties(velocypack::Builder& builder,
                                     Serialization context) const {
  if (!builder.isOpenObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "invalid builder provided for data-source definition");
  }

  builder.add(StaticStrings::DataSourceGuid,
              toValuePair(guid()));  // required for dump/restore
  builder.add(StaticStrings::DataSourceId, velocypack::Value(std::to_string(id().id())));
  builder.add(StaticStrings::DataSourceName, toValuePair(name()));

  // note: includeSystem and forPersistence are not 100% synonymous,
  // however, for our purposes this is an okay mapping; we only set
  // includeSystem if we are persisting the properties
  if (context == Serialization::Persistence || context == Serialization::PersistenceWithInProgress) {
    builder.add(StaticStrings::DataSourceDeleted, velocypack::Value(deleted()));
    builder.add(StaticStrings::DataSourceSystem, velocypack::Value(system()));

    // FIXME not sure if the following is relevant
    // Cluster Specific
    builder.add(StaticStrings::DataSourcePlanId,
                velocypack::Value(std::to_string(planId().id())));
  }

  return appendVelocyPack(builder, context);
}

}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
