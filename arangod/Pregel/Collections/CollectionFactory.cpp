#include "CollectionFactory.h"
#include <memory>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Collections/Collections.h"
#include "Pregel/Collections/ClusterCollection.h"
#include "Pregel/Collections/SingleServerCollection.h"

using namespace arangodb::pregel::collections;

auto CollectionFactory::create(std::vector<CollectionID> names)
    -> ResultT<Collections> {
  ServerState* ss = ServerState::instance();
  if (ss->getRole() == ServerState::ROLE_SINGLE) {
    auto collections =
        std::unordered_map<CollectionID, std::shared_ptr<Collection>>{};
    for (auto const& name : names) {
      auto collection = vocbase.lookupCollection(name);

      if (collection == nullptr) {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
      }
      collections[name] =
          std::make_shared<SingleServerCollection>(std::move(collection));
    }
    return {Collections{std::move(collections)}};
  } else {
    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    auto collections =
        std::unordered_map<CollectionID, std::shared_ptr<Collection>>{};
    for (auto const& name : names) {
      auto collection = ci.getCollectionNT(vocbase.name(), name);
      if (collection == nullptr) {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
      }
      // smart edge collections contain multiple actual collections
      std::vector<std::string> internalNames = collection->realNamesForRead();
      auto internalCollections =
          std::vector<std::shared_ptr<LogicalCollection>>{};
      for (auto const& internalName : internalNames) {
        auto internalCollection =
            ci.getCollectionNT(vocbase.name(), internalName);
        if (internalCollection == nullptr) {
          return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
        }
        internalCollections.emplace_back(internalCollection);
      }
      collections[name] = std::make_shared<ClusterCollection>(
          collection->name(), std::move(internalCollections), ci);
    }
    return {Collections{std::move(collections)}};
  }
}
