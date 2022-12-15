#include "SingleServerCollection.h"
#include "Cluster/ServerState.h"

using namespace arangodb::pregel::collections;

auto SingleServerCollection::name() const -> std::string_view {
  return collection->name();
}

auto SingleServerCollection::shards() const -> std::vector<ShardID> {
  return {collection->name()};
}

auto SingleServerCollection::shardsPerServer() const
    -> std::unordered_map<ServerID,
                          std::map<CollectionID, std::vector<ShardID>>> {
  ServerState* ss = ServerState::instance();
  return {{ss->getId(), {{collection->name(), shards()}}}};
}

auto SingleServerCollection::planIds() const
    -> std::unordered_map<arangodb::CollectionID, std::string> {
  return {{collection->name(), std::to_string(collection->planId().id())}};
}

auto SingleServerCollection::isSystem() const -> bool {
  return collection->system();
}

auto SingleServerCollection::isDeleted() const -> bool {
  return collection->deleted() ||
         collection->status() == TRI_VOC_COL_STATUS_DELETED;
}

auto SingleServerCollection::hasAccessRights(auth::Level requested) -> bool {
  ExecContext const& exec = ExecContext::current();
  return exec.isSuperuser() ||
         exec.canUseCollection(collection->name(), requested);
}

auto SingleServerCollection::isSmart() const -> bool {
  return collection->isSmart();
}

auto SingleServerCollection::shardKeys() const -> std::vector<std::string> {
  return collection->shardKeys();
}
