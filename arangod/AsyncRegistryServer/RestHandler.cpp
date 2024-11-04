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

#include "Async/Registry/promise.h"
#include "Async/Registry/stacktrace.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Async/Registry/promise.h"
#include "Async/Registry/registry_variable.h"
#include "Inspection/VPack.h"
#include "Rest/CommonDefines.h"

using namespace arangodb;
using namespace arangodb::async_registry;

auto all_undeleted_promises()
    -> std::pair<WaiterForest<PromiseSnapshot>, std::vector<Id>> {
  WaiterForest<PromiseSnapshot> forest;
  std::vector<Id> roots;
  registry.for_promise([&](PromiseSnapshot promise) {
    if (promise.state != State::Deleted) {
      forest.insert(promise.id, promise.waiter, promise);
      if (promise.waiter == nullptr) {
        roots.emplace_back(promise.id);
      }
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

auto RestHandler::execute() -> RestStatus {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

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

  generateResult(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}
