#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "Aql/TraversalStats.h"
#include "Cluster/ClusterTypes.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Network/Methods.h"

namespace arangodb::graph {

template<typename Step>
struct ClusterNeighbourCursor {
  using EngineRequest = std::pair<ServerID, futures::Future<network::Response>>;
  /**
     Creates cursor and sends neighbour first requests asynchronously to
     dbservers
   */
  ClusterNeighbourCursor(Step step, size_t position,
                         arangodb::transaction::Methods* trx,
                         ClusterBaseProviderOptions& options,
                         aql::TraversalStats& stats);

  /**
     Gets next batch of steps

     Internally, waits for previously send neighbour requests, returns these and
     if the cursor is not yet done (response tells that) it sends continuation
     requests asynchronously to dbservers.
   */
  auto next() -> std::vector<Step>;
  auto hasMore() -> bool;
  auto markForDeletion() -> void { _deletable = true; };

 public:
  template<typename S, typename Inspector>
  friend auto inspect(Inspector& f, ClusterNeighbourCursor<S>& x);
  bool _deletable = false;

 private:
  Step _step;
  size_t _position;
  std::vector<EngineRequest> _requests = {};
  arangodb::transaction::Methods* _trx;
  ClusterBaseProviderOptions& _opts;
  aql::TraversalStats& _stats;
};
template<typename Step, typename Inspector>
auto inspect(Inspector& f, ClusterNeighbourCursor<Step>& x) {
  return f.object(x).fields(f.field("step", x._step));
}

}  // namespace arangodb::graph
