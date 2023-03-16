#include "ResolveShard.h"

#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;

ErrorCode ResolveShard::resolve(ClusterInfo& ci, WorkerConfig const* config,
                                std::string const& collectionName,
                                std::string const& shardKey,
                                std::string_view vertexKey,
                                std::string& responsibleShard) {
  if (!ServerState::instance()->isRunningInCluster()) {
    responsibleShard = collectionName;
    return TRI_ERROR_NO_ERROR;
  }

  auto const& planIDMap = config->collectionPlanIdMap();
  std::shared_ptr<LogicalCollection> info;
  auto const& it = planIDMap.find(collectionName);
  if (it != planIDMap.end()) {
    info = ci.getCollectionNT(config->database(), it->second);  // might throw
    if (info == nullptr) {
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
    }
  } else {
    LOG_TOPIC("67fda", ERR, Logger::PREGEL)
        << "The collection could not be translated to a planID";
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  TRI_ASSERT(info != nullptr);

  VPackBuilder partial;
  partial.openObject();
  partial.add(shardKey, VPackValuePair(vertexKey.data(), vertexKey.size(),
                                       VPackValueType::String));
  partial.close();
  //  LOG_TOPIC("00a5c", INFO, Logger::PREGEL) << "Partial doc: " <<
  //  partial.toJson();
  return info->getResponsibleShard(partial.slice(), false, responsibleShard);
}
