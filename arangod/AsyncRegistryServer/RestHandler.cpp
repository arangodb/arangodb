////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#include "RestHandler.h"
#include <optional>
#include <variant>

#include "Async/Registry/promise.h"
#include "Async/Registry/stacktrace.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Async/Registry/promise.h"
#include "Async/Registry/registry_variable.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Inspection/VPack.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/RequestOptions.h"
#include "Rest/CommonDefines.h"

using namespace arangodb;
using namespace arangodb::async_registry;

auto all_undeleted_promises()
    -> std::pair<WaiterForest<PromiseSnapshot>, std::vector<Id>> {
  WaiterForest<PromiseSnapshot> forest;
  std::vector<Id> roots;
  registry.for_promise([&](PromiseSnapshot promise) {
    if (promise.state != State::Deleted) {
      // only async waiters are considered here, they point to other promises in
      // the registry
      // sync waiters point to threads that are waiting synchronously and are
      // not relevant for creating the forest
      std::visit(overloaded{
                     [&](PromiseId waiter) {
                       forest.insert(promise.id, waiter, promise);
                     },
                     [&](ThreadId thread) {
                       forest.insert(promise.id, nullptr, promise);
                       roots.emplace_back(promise.id);
                     },
                 },
                 promise.requester);
    }
  });
  return std::make_pair(forest, roots);
}

struct Entry {
  TreeHierarchy hierarchy;
  PromiseSnapshot data;
};
template<typename Inspector>
auto inspect(Inspector& f, Entry& x) {
  return f.object(x).fields(f.field("hierarchy", x.hierarchy),
                            f.field("data", x.data));
}

RestHandler::RestHandler(ArangodServer& server, GeneralRequest* request,
                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()) {}

namespace {
auto getData() -> VPackBuilder {
  auto [promises, roots] = all_undeleted_promises();
  auto awaited_promises = promises.index_by_awaitee();

  VPackBuilder builder;
  builder.openObject();
  builder.add(VPackValue("promise_stacktraces"));
  builder.openArray();
  for (auto root : roots) {
    builder.openArray();
    auto dfs = DFS_PostOrder{awaited_promises, root};
    do {
      auto next = dfs.next();
      if (next == std::nullopt) {
        break;
      }
      auto [id, hierarchy] = next.value();
      auto data = awaited_promises.data(id);
      if (data != std::nullopt) {
        auto entry = Entry{.hierarchy = hierarchy, .data = data.value()};
        velocypack::serialize(builder, entry);
      }
    } while (true);
    builder.close();
  }
  builder.close();
  builder.close();
  return builder;
}
}  // namespace

auto RestHandler::executeAsync() -> futures::Future<futures::Unit> {
  if (!ExecContext::current().isSuperuser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "you need super user rights for log operations");
  }

  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    co_return;
  }

  bool foundServerIdParameter;
  std::string const& serverId =
      _request->value("serverId", foundServerIdParameter);

  if (ServerState::instance()->isCoordinator() && foundServerIdParameter) {
    if (serverId != ServerState::instance()->getId()) {
      // not ourselves! - need to pass through the request
      auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

      bool found = false;
      for (auto const& srv : ci.getServers()) {
        // validate if server id exists
        if (srv.first == serverId) {
          found = true;
          break;
        }
      }

      if (!found) {
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_BAD_PARAMETER,
                      "unknown serverId supplied.");
        co_return;
      }

      NetworkFeature const& nf = server().getFeature<NetworkFeature>();
      network::ConnectionPool* pool = nf.pool();
      if (pool == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }
      network::RequestOptions options;
      options.timeout = network::Timeout(30.0);
      options.database = _request->databaseName();
      options.parameters = _request->parameters();

      auto f = network::sendRequestRetry(
          pool, "server:" + serverId, fuerte::RestVerb::Get,
          _request->requestPath(), VPackBuffer<uint8_t>{}, options);
      co_await std::move(f).thenValue(
          [self = std::dynamic_pointer_cast<RestHandler>(shared_from_this())](
              network::Response const& r) {
            if (r.fail()) {
              self->generateError(r.combinedResult());
            } else {
              self->generateResult(rest::ResponseCode::OK, r.slice());
            }
          });
      co_return;
    }
  }

  auto lock_guard = co_await _feature.asyncLock();
  generateResult(rest::ResponseCode::OK, getData().slice());
  co_return;
}
