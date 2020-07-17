////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CollectionNameResolver.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/NumberUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

namespace {
std::string const UNKNOWN("_unknown");
}

namespace arangodb {

// copy an existing resolver
CollectionNameResolver::CollectionNameResolver(CollectionNameResolver const& other) 
    : CollectionNameResolver(other._vocbase) {
  READ_LOCKER(locker, other._lock);
  _resolvedIds = other._resolvedIds;
  _dataSourceById = other._dataSourceById;
  _dataSourceByName = other._dataSourceByName;
}

std::shared_ptr<LogicalCollection> CollectionNameResolver::getCollection(TRI_voc_cid_t id) const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<LogicalCollection>(getDataSource(id));
#else
  auto dataSource = getDataSource(id);

  return dataSource && dataSource->category() == LogicalCollection::category()
             ? std::static_pointer_cast<LogicalCollection>(dataSource)
             : nullptr;
#endif
}

std::shared_ptr<LogicalCollection> CollectionNameResolver::getCollection(std::string const& nameOrId) const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<LogicalCollection>(getDataSource(nameOrId));
#else
  auto dataSource = getDataSource(nameOrId);

  return dataSource && dataSource->category() == LogicalCollection::category()
             ? std::static_pointer_cast<LogicalCollection>(dataSource)
             : nullptr;
#endif
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection id for a collection name (local case),
/// use this if you know you are on a single server or on a DBserver
/// and need to look up a local collection name (or shard name).
//////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t CollectionNameResolver::getCollectionIdLocal(std::string const& name) const {
  if (!name.empty()) {
    if (name[0] >= '0' && name[0] <= '9') {
      // name is a numeric id
      return NumberUtils::atoi_zero<TRI_voc_cid_t>(name.data(), name.data() + name.size());
    }

    auto ds = _vocbase.lookupDataSource(name);

    if (ds != nullptr) {
      return ds->id();
    }
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster collection id for a cluster collection name,
/// only use this is in cluster mode on a coordinator or DBserver, in both
/// cases the name is resolved as a cluster wide collection name and the
/// cluster wide collection id is returned.
//////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t CollectionNameResolver::getCollectionIdCluster(std::string const& name) const {
  if (!ServerState::isRunningInCluster(_serverRole)) {
    return getCollectionIdLocal(name);
  }
  if (name.empty()) {
    return 0;
  }
  if (name[0] >= '0' && name[0] <= '9') {
    // name is a numeric id
    TRI_voc_cid_t cid =
        NumberUtils::atoi_zero<TRI_voc_cid_t>(name.data(), name.data() + name.size());
    auto collection = getCollection(cid);
    // Now validate the cid
    auto type = collection ? collection->type() : TRI_COL_TYPE_UNKNOWN;

    if (type == TRI_COL_TYPE_UNKNOWN) {
      return 0;
    }
    return cid;
  }

  try {
    // We have to look up the collection info:
    if (_vocbase.server().hasFeature<ClusterFeature>()) {
      auto& ci = _vocbase.server().getFeature<ClusterFeature>().clusterInfo();

      auto const info = ci.getCollectionOrViewNT(_vocbase.name(), name);

      if (info != nullptr) {
        return info->id();
      }
    }
    // fallthrough to returning 0
  } catch (...) {
  }

  return 0;
}

std::shared_ptr<LogicalCollection> CollectionNameResolver::getCollectionStructCluster(
    std::string const& name) const {
  if (!ServerState::isRunningInCluster(_serverRole)) {
    return _vocbase.lookupCollection(name);
  }

  // We have to look up the collection info:
  return _vocbase.server().hasFeature<ClusterFeature>()
             ? _vocbase.server().getFeature<ClusterFeature>().clusterInfo().getCollectionNT(
                   _vocbase.name(), name)
             : nullptr;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection id for a collection name, this is the
/// default one to use, which will usually do the right thing. On a
/// single server or DBserver it will use the local lookup and on a
/// coordinator it will use the cluster wide lookup.
//////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t CollectionNameResolver::getCollectionId(std::string const& name) const {
  if (!ServerState::isRunningInCluster(_serverRole) || ServerState::isDBServer(_serverRole)) {
    return getCollectionIdLocal(name);
  }
  return getCollectionIdCluster(name);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection name for a collection id, this implements
/// some magic in the cluster case: a DBserver in a cluster will automatically
/// translate the local collection ID into a cluster wide collection name.
//////////////////////////////////////////////////////////////////////////////

std::string CollectionNameResolver::getCollectionName(TRI_voc_cid_t cid) const {
  {
    READ_LOCKER(locker, _lock);
    auto it = _resolvedIds.find(cid);

    if (it != _resolvedIds.end()) {
      return (*it).second;
    }
  }

  std::string name = lookupName(cid);
  {
    WRITE_LOCKER(locker, _lock);
    _resolvedIds.emplace(cid, name);
  }

  return name;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster-wide collection name for a cluster-wide
/// collection id
//////////////////////////////////////////////////////////////////////////////

std::string CollectionNameResolver::getCollectionNameCluster(TRI_voc_cid_t cid) const {
  if (!ServerState::isClusterRole(_serverRole)) {
    // This handles the case of a standalone server
    return getCollectionName(cid);
  }

  // First check the cache:
  {
    READ_LOCKER(locker, _lock);
    auto it = _resolvedIds.find(cid);

    if (it != _resolvedIds.end()) {
      return (*it).second;
    }
  }

  std::string name(::UNKNOWN);

  if (ServerState::isDBServer(_serverRole)) {
    // This might be a local system collection:
    name = lookupName(cid);
  }

  if (name == ::UNKNOWN) {
    auto ci = _vocbase.server().getFeature<ClusterFeature>().clusterInfo().getCollectionNT(
        _vocbase.name(), arangodb::basics::StringUtils::itoa(cid));
    if (ci != nullptr) {
      name = ci->name();
    }
  }

  LOG_TOPIC_IF("817e8", DEBUG, arangodb::Logger::FIXME, name == ::UNKNOWN)
      << "CollectionNameResolver: was not able to resolve id " << cid;
  WRITE_LOCKER(locker, _lock);
  _resolvedIds.emplace(cid, name);
  return name;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return collection name if given string is either the name or
/// a string with the (numerical) collection id, this returns the cluster
/// wide collection name in the DBserver case
//////////////////////////////////////////////////////////////////////////////

std::string CollectionNameResolver::getCollectionName(std::string const& nameOrId) const {
  if (!nameOrId.empty() && (nameOrId[0] < '0' || nameOrId[0] > '9')) {
    return nameOrId;
  }
  return getCollectionName(
      NumberUtils::atoi_zero<TRI_voc_cid_t>(nameOrId.data(),
                                            nameOrId.data() + nameOrId.size()));
}

std::shared_ptr<LogicalDataSource> CollectionNameResolver::getDataSource(TRI_voc_cid_t id) const {
  decltype(_dataSourceById)::iterator itr;

  {
    READ_LOCKER(guard, _lock);
    itr = _dataSourceById.find(id);
    if (itr != _dataSourceById.end()) {
      return itr->second;
    }
  }
  
  // db server / standalone
  auto ptr = ServerState::isCoordinator(_serverRole)
                 ? getDataSource(std::to_string(id))
                 : _vocbase.lookupDataSource(id);

  if (ptr) {
    WRITE_LOCKER(guard, _lock);
    _dataSourceById.emplace(id, ptr);
  }

  return ptr;
}

std::shared_ptr<LogicalDataSource> CollectionNameResolver::getDataSource(std::string const& nameOrId) const {
  decltype(_dataSourceByName)::iterator itr;
  
  {
    READ_LOCKER(guard, _lock);
    itr = _dataSourceByName.find(nameOrId);

    if (itr != _dataSourceByName.end()) {
      return itr->second;
    }
  }

  std::shared_ptr<LogicalDataSource> ptr;

  // db server / standalone
  if (!ServerState::isCoordinator(_serverRole)) {
    ptr = _vocbase.lookupDataSource(nameOrId);
  } else {
    // cluster coordinator
    if (!_vocbase.server().hasFeature<ClusterFeature>()) {
      return nullptr;
    }
    auto& ci = _vocbase.server().getFeature<ClusterFeature>().clusterInfo();

    ptr = ci.getCollectionOrViewNT(_vocbase.name(), nameOrId);
  }

  if (ptr) {
    WRITE_LOCKER(guard, _lock);
    _dataSourceByName.emplace(nameOrId, ptr);
  }

  return ptr;
}

std::shared_ptr<LogicalView> CollectionNameResolver::getView(TRI_voc_cid_t id) const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<LogicalView>(getDataSource(id));
#else
  auto dataSource = getDataSource(id);

  return dataSource && dataSource->category() == LogicalView::category()
             ? std::static_pointer_cast<LogicalView>(dataSource)
             : nullptr;
#endif
}

std::shared_ptr<LogicalView> CollectionNameResolver::getView(std::string const& nameOrId) const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<LogicalView>(getDataSource(nameOrId));
#else
  auto dataSource = getDataSource(nameOrId);

  return dataSource && dataSource->category() == LogicalView::category()
             ? std::static_pointer_cast<LogicalView>(dataSource)
             : nullptr;
#endif
}

bool CollectionNameResolver::visitCollections(std::function<bool(LogicalCollection&)> const& visitor,
                                              TRI_voc_cid_t id) const {
  auto dataSource = getDataSource(id);

  if (!dataSource) {
    return false;  // no way to determine what to visit
  }

  if (LogicalCollection::category() == dataSource->category()) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto collection = std::dynamic_pointer_cast<LogicalCollection>(dataSource);
    TRI_ASSERT(collection);
#else
    auto collection = std::static_pointer_cast<LogicalCollection>(dataSource);
#endif

    // TODO resolve smart edge collection CIDs here
    return visitor(*collection);
  }

  if (LogicalView::category() == dataSource->category()) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto view = std::dynamic_pointer_cast<LogicalView>(dataSource);
#else
    auto view = std::static_pointer_cast<LogicalView>(dataSource);
#endif

    // each CID in a view might need further resolution
    return view->visitCollections([this, &visitor, id](TRI_voc_cid_t cid) -> bool {
      return cid == id ? false : visitCollections(visitor, cid);  // avoid infinite recursion
    });
  }

  return false;  // no way to determine what to visit
}

/// PRIVATE ---------------
std::string CollectionNameResolver::lookupName(TRI_voc_cid_t cid) const {
  auto collection = _vocbase.lookupCollection(cid);

  // exactly as in the non-cluster case
  if (!ServerState::isDBServer(_serverRole)) {
    return collection ? collection->name() : ::UNKNOWN;
  }

  // DBserver case of a shard:
  if (collection && collection->planId() != collection->id()) {
    collection = _vocbase.server().getFeature<ClusterFeature>().clusterInfo().getCollectionNT(
        collection->vocbase().name(), std::to_string(collection->planId()));
  }

  // can be empty, if collection unknown
  if (collection != nullptr && !collection->name().empty()) {
    return collection->name();
  }

  return ::UNKNOWN;
}

}  // namespace arangodb
