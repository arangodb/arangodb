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

#include "Basics/Common.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection id for a collection name (local case),
/// use this if you know you are on a single server or on a DBserver
/// and need to look up a local collection name (or shard name).
//////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t CollectionNameResolver::getCollectionIdLocal(
    std::string const& name) const {
  if (name[0] >= '0' && name[0] <= '9') {
    // name is a numeric id
    return static_cast<TRI_voc_cid_t>(
        arangodb::basics::StringUtils::uint64(name));
  }

  arangodb::LogicalCollection const* collection = getCollectionStruct(name);

  if (collection != nullptr) {
    return collection->cid();
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster collection id for a cluster collection name,
/// only use this is in cluster mode on a coordinator or DBserver, in both
/// cases the name is resolved as a cluster wide collection name and the
/// cluster wide collection id is returned.
//////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t CollectionNameResolver::getCollectionIdCluster(
    std::string const& name) const {
  if (!ServerState::isRunningInCluster(_serverRole)) {
    return getCollectionIdLocal(name);
  }
  if (name[0] >= '0' && name[0] <= '9') {
    // name is a numeric id
    TRI_voc_cid_t cid = static_cast<TRI_voc_cid_t>(arangodb::basics::StringUtils::uint64(name));
    // Now validate the cid
    TRI_col_type_e type = getCollectionTypeCluster(getCollectionNameCluster(cid));
    if (type == TRI_COL_TYPE_UNKNOWN) {
      return 0;
    }
    return cid;
  }

  try {
    // We have to look up the collection info:
    ClusterInfo* ci = ClusterInfo::instance();
    auto cinfo = ci->getCollection(_vocbase->name(), name);
    TRI_ASSERT(cinfo != nullptr);
    return cinfo->cid();
  } catch (...) {
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection id for a collection name, this is the
/// default one to use, which will usually do the right thing. On a
/// single server or DBserver it will use the local lookup and on a
/// coordinator it will use the cluster wide lookup.
//////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t CollectionNameResolver::getCollectionId(
    std::string const& name) const {
  if (!ServerState::isRunningInCluster(_serverRole) ||
      ServerState::isDBServer(_serverRole)) {
    return getCollectionIdLocal(name);
  }
  return getCollectionIdCluster(name);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection type for a collection name (local case)
//////////////////////////////////////////////////////////////////////////////

TRI_col_type_e CollectionNameResolver::getCollectionType(
    std::string const& name) const {
  if (name[0] >= '0' && name[0] <= '9') {
    // name is a numeric id
    return getCollectionType(getCollectionName(static_cast<TRI_voc_cid_t>(
        arangodb::basics::StringUtils::uint64(name))));
  }

  arangodb::LogicalCollection const* collection = getCollectionStruct(name);

  if (collection != nullptr) {
    return collection->type();
  }
  return TRI_COL_TYPE_UNKNOWN;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection struct for a collection name
//////////////////////////////////////////////////////////////////////////////

arangodb::LogicalCollection const* CollectionNameResolver::getCollectionStruct(
    std::string const& name) const {
  auto it = _resolvedNames.find(name);

  if (it != _resolvedNames.end()) {
    return (*it).second;
  }

  arangodb::LogicalCollection const* collection = _vocbase->lookupCollection(name);

  if (collection != nullptr) {
    _resolvedNames.emplace(name, collection);
  }

  return collection;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster collection type for a cluster collection name on
/// the
///        coordinator and for a shard name on the db server
//////////////////////////////////////////////////////////////////////////////

TRI_col_type_e CollectionNameResolver::getCollectionTypeCluster(
    std::string const& name) const {
  // This fires in Single server case as well
  if (!ServerState::isCoordinator(_serverRole)) {
    return getCollectionType(name);
  }
  if (name[0] >= '0' && name[0] <= '9') {
    // name is a numeric id
    return getCollectionTypeCluster(
        getCollectionName(static_cast<TRI_voc_cid_t>(
            arangodb::basics::StringUtils::uint64(name))));
  }

  try {
    // We have to look up the collection info:
    ClusterInfo* ci = ClusterInfo::instance();
    auto cinfo = ci->getCollection(_vocbase->name(), name);
    TRI_ASSERT(cinfo != nullptr);
    return cinfo->type();
  } catch(...) {
  }
  return TRI_COL_TYPE_UNKNOWN;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a collection name for a collection id, this implements
/// some magic in the cluster case: a DBserver in a cluster will automatically
/// translate the local collection ID into a cluster wide collection name.
//////////////////////////////////////////////////////////////////////////////

std::string CollectionNameResolver::getCollectionName(TRI_voc_cid_t cid) const {
  auto it = _resolvedIds.find(cid);

  if (it != _resolvedIds.end()) {
    return (*it).second;
  }

  std::string name = localNameLookup(cid);
  _resolvedIds.emplace(cid, name);

  return name;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look up a cluster-wide collection name for a cluster-wide
/// collection id
//////////////////////////////////////////////////////////////////////////////

std::string CollectionNameResolver::getCollectionNameCluster(
    TRI_voc_cid_t cid) const {
  // First check the cache:
  auto it = _resolvedIds.find(cid);

  if (it != _resolvedIds.end()) {
    return (*it).second;
  }

  if (!ServerState::isClusterRole(_serverRole)) {
    // This handles the case of a standalone server
    return getCollectionName(cid);
  }

  std::string name;
  
  if (ServerState::isDBServer(_serverRole)) {
    // This might be a local system collection:
    name = localNameLookup(cid);
    if (name != "_unknown") {
      _resolvedIds.emplace(cid, name);
      return name;
    }
  }

  int tries = 0;

  while (tries++ < 2) {
    auto ci = ClusterInfo::instance()->getCollection(
        _vocbase->name(), arangodb::basics::StringUtils::itoa(cid));

    if (ci == nullptr) {
      ClusterInfo::instance()->flush();
      continue;
    }
    name = ci->name();
    _resolvedIds.emplace(cid, name);
    return name;
  }

  LOG(ERR) << "CollectionNameResolver: was not able to resolve id " << cid;
  return "_unknown";
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return collection name if given string is either the name or
/// a string with the (numerical) collection id, this returns the cluster
/// wide collection name in the DBserver case
//////////////////////////////////////////////////////////////////////////////

std::string CollectionNameResolver::getCollectionName(
    std::string const& nameOrId) const {
  if (!nameOrId.empty() &&
      (nameOrId[0] < '0' || nameOrId[0] > '9')) {
    return nameOrId;
  }
  TRI_voc_cid_t tmp = arangodb::basics::StringUtils::uint64(nameOrId);
  return getCollectionName(tmp);
}

std::string CollectionNameResolver::localNameLookup(TRI_voc_cid_t cid) const {
  std::string name;

  if (ServerState::isDBServer(_serverRole)) {
    READ_LOCKER(readLocker, _vocbase->_collectionsLock);

    auto it = _vocbase->_collectionsById.find(cid);

    if (it != _vocbase->_collectionsById.end()) {
      if ((*it).second->planId() == (*it).second->cid()) {
        // DBserver local case
        name = (*it).second->name();
      } else {
        // DBserver case of a shard:
        name = arangodb::basics::StringUtils::itoa((*it).second->planId());
        auto ci = ClusterInfo::instance()->getCollection(
            (*it).second->dbName(), name);
        if (ci == nullptr) {
          name = ""; // collection unknown
        } else {
          name = ci->name();  // can be empty, if collection unknown
        }
      }
    }
  } else {
    // exactly as in the non-cluster case
    name = _vocbase->collectionName(cid);
  }

  if (name.empty()) {
    name = "_unknown";
  }
  return name;
}
