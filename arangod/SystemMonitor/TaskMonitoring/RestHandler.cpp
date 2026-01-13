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

#include "Containers/Forest/depth_first.h"
#include "Containers/Forest/forest.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "TaskMonitoring/task.h"
#include "TaskMonitoring/task_registry_variable.h"
#include "Inspection/VPack.h"

using namespace arangodb;
using namespace arangodb::task_monitoring;
using namespace arangodb::containers;

RestHandler::RestHandler(ArangodServer& server, GeneralRequest* request,
                         GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _feature(server.getFeature<Feature>()) {}

namespace {
struct Entry {
  TreeHierarchy hierarchy;
  TaskSnapshot data;
};
template<typename Inspector>
auto inspect(Inspector& f, Entry& x) {
  return f.object(x).fields(f.field("hierarchy", x.hierarchy),
                            f.field("data", x.data));
}
/**
   Creates a forest of all current tasks

   An edge between two tasks means that the lower hierarchy tasks started the
 larger hierarchy task.
 **/
auto all_undeleted_tasks() -> ForestWithRoots<TaskSnapshot> {
  auto forest = Forest<TaskSnapshot>{};
  std::vector<Id> roots;
  registry.for_node([&](TaskSnapshot task) {
    if (task.state != State::Deleted) {
      std::visit(overloaded{
                     [&](TaskId parent) {
                       forest.insert(task.id.id, parent.id, task);
                     },
                     [&](RootTask root) {
                       forest.insert(task.id.id, nullptr, task);
                       roots.emplace_back(task.id.id);
                     },
                 },
                 task.parent);
    }
  });
  return ForestWithRoots{forest, roots};
}

/**
   Serializes a task dependency-forest into a list of trees.

   Each tree is given as a list of tasks, where its hierachy number and position
 inside the list defines its location in the tree. To create one tree, it uses a
 depth first search to traverse the forest in post order, such that tasks with
 the highest hierarchy in a tree are given first and the root task with
 hierarchy zero is given last.
 **/
auto serialize(IndexedForestWithRoots<TaskSnapshot> const& tasks)
    -> VPackBuilder {
  VPackBuilder builder;
  builder.openObject();
  builder.add(VPackValue("task_stacktraces"));
  builder.openArray();
  for (auto const& root : tasks.roots()) {
    builder.openArray();
    auto dfs = DFS_PostOrder{tasks, root};
    do {
      auto next = dfs.next();
      if (next == std::nullopt) {
        break;
      }
      auto [id, hierarchy] = next.value();
      auto data = tasks.node(id);
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
                  "you need admin user rights for task monitoring operations");
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

  auto tasks = all_undeleted_tasks().index_by_parent();
  generateResult(rest::ResponseCode::OK, serialize(tasks).slice());
  co_return;
}
