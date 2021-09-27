#include "CollectionValidator.h"
#include <Basics/StaticStrings.h>
#include <Basics/VelocyPackHelper.h>
#include <Cluster/ClusterFeature.h>
#include <Utilities/NameValidator.h>
#include <Utils/Events.h>
#include <VocBase/vocbase.h>
#include "Sharding/ShardingInfo.h"

using namespace arangodb;

Result CollectionValidator::validateCreationInfo() {
  // check whether the name of the collection is valid
  if (CollectionNameValidator::isAllowedName(false, false, _vocbase.name())) { // todo check second false
    events::CreateCollection(_vocbase.name(), _info.name, TRI_ERROR_ARANGO_ILLEGAL_NAME);
    return {TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }

  // check the collection type in \p _info
  if (_info.collectionType != TRI_col_type_e::TRI_COL_TYPE_DOCUMENT &&
      _info.collectionType != TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    events::CreateCollection(_vocbase.name(), _info.name, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return {TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID};
  }

  // validate shards factor and replication factor
  if (ServerState::instance()->isCoordinator() || _isSingleServerSmartGraph) {
    Result res = ShardingInfo::validateShardsAndReplicationFactor(_info.properties, _vocbase.server(), _enforceReplicationFactor);
    if (res.fail()) {
      return res;
    }
  }


  // All collections on a single server should be local collections.
  // A Coordinator should never have local collections.
  // On an Agent, all collections should be local collections.
  // On a DBServer, the only local collections should be system collections
  // (like _statisticsRaw). Non-local (system or not) collections are shards,
  // so don't have system-names, even if they are system collections!
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_SINGLE:
      TRI_ASSERT(_isLocalCollection);
      break;
    case ServerState::ROLE_DBSERVER:
      TRI_ASSERT(_isLocalCollection == _isSystemName);
      break;
    case ServerState::ROLE_COORDINATOR:
      TRI_ASSERT(!_isLocalCollection);
      break;
    case ServerState::ROLE_AGENT:
      TRI_ASSERT(_isLocalCollection);
      break;
    case ServerState::ROLE_UNDEFINED:
      TRI_ASSERT(false);
  }

  if (_isLocalCollection && !_isSingleServerSmartGraph) {
    // the combination "isSmart" and replicationFactor "satellite" does not make any sense.
    // note: replicationFactor "satellite" can also be expressed as replicationFactor 0.
    VPackSlice s = _info.properties.get(StaticStrings::IsSmart);
    auto replicationFactorSlice = _info.properties.get(StaticStrings::ReplicationFactor);
    if (s.isBoolean() && s.getBoolean() &&
        ((replicationFactorSlice.isNumber() &&
          replicationFactorSlice.getNumber<int>() == 0) ||
         (replicationFactorSlice.isString() &&
          replicationFactorSlice.stringRef() == StaticStrings::Satellite))) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid combination of 'isSmart' and 'satellite' "
              "replicationFactor"};
    }
  }





  return {TRI_ERROR_NO_ERROR};
}
