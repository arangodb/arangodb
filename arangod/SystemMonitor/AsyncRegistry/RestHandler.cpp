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
#include "Containers/Forest/depth_first.h"
#include "Containers/Forest/forest.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Async/Registry/promise.h"
#include "Async/Registry/registry_variable.h"
#include "Inspection/VPack.h"

using namespace arangodb;
using namespace arangodb::async_registry;
using namespace arangodb::containers;

RestHandler::RestHandler(ArangodServer& server, GeneralRequest* request,
                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()) {}

namespace {
struct Entry {
  TreeHierarchy hierarchy;
  PromiseSnapshot data;
};
template<typename Inspector>
auto inspect(Inspector& f, Entry& x) {
  return f.object(x).fields(f.field("hierarchy", x.hierarchy),
                            f.field("data", x.data));
}
/**
   Creates a forest of all promises in the async registry

   An edge between two promises means that the lower hierarchy promise waits for
 the larger hierarchy promise.
 **/
auto all_undeleted_promises() -> ForestWithRoots<PromiseSnapshot> {
  Forest<PromiseSnapshot> forest;
  std::vector<Id> roots;
  registry.for_node([&](PromiseSnapshot promise) {
    if (promise.state != State::Deleted) {
      std::visit(overloaded{
                     [&](PromiseId const& async_waiter) {
                       forest.insert(promise.id.id, async_waiter.id, promise);
                     },
                     [&](basics::ThreadInfo const& sync_waiter_thread) {
                       forest.insert(promise.id.id, nullptr, promise);
                       roots.emplace_back(promise.id.id);
                     },
                 },
                 promise.requester);
    }
  });
  return ForestWithRoots{forest, roots};
}

/**
   Converts a forest of promises into a list of stacktraces inside a
 velocypack.

   The list of stacktraces include one stacktrace per tree in the forest. To
 create one stacktrace, it uses a depth first search to traverse the forest in
 post order, such that promises with the highest hierarchy in a tree are given
 first and the root promise is given last.
 **/
auto serialize(IndexedForestWithRoots<PromiseSnapshot> const& promises)
    -> VPackBuilder {
  VPackBuilder builder;
  builder.openObject();
  builder.add(VPackValue("promise_stacktraces"));
  builder.openArray();
  for (auto const& root : promises.roots()) {
    builder.openArray();
    auto dfs = DFS_PostOrder{promises, root};
    do {
      auto next = dfs.next();
      if (next == std::nullopt) {
        break;
      }
      auto [id, hierarchy] = next.value();
      auto data = promises.node(id);
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
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "you need admin user rights for async-registry operations");
    co_return;
  }

  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    co_return;
  }

  auto isForwarded = co_await tryForwarding();
  if (isForwarded) {
    co_return;
  }

  auto lock_guard = co_await _feature.asyncLock();

  auto promises = all_undeleted_promises().index_by_parent();
  generateResult(rest::ResponseCode::OK, serialize(promises).slice());
  co_return;
}
