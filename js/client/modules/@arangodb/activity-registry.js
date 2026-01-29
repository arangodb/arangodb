// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

const db = require('internal').db;
const arangosh = require('@arangodb/arangosh');
const IM = global.instanceManager;
const internal = require('internal');

const ACTIVITY_REGISTRY_URL = '/_admin/activity-registry';

exports.get_snapshot = function (server) {
  if (server === undefined) {
    return arangosh.checkRequestResult(db._connection.GET(ACTIVITY_REGISTRY_URL));
  }
  IM.rememberConnection();
  IM.arangods.filter((x) => x.id === server)[0].connect();
  const result = arangosh.checkRequestResult(internal.arango.GET_RAW(ACTIVITY_REGISTRY_URL)).parsedBody;
  IM.reconnectMe();
  return result;
};

exports.pretty_print = function (activities) {
  // TODO put them in a forest structure
  
  // TODO traverse each tree in pre-order
  // TODO how to be able to print items with stale parents (have no root)
  return activities
    .map((a) => `── ${a.name}: ${JSON.stringify(a.metadata)}`)
    .join("\n");
}

exports.createForest = function (activities) {
  const groupedByParent = Map.groupBy(activities, (a) => a.parent); // parent_id -> [child activity, child activity]
  // print(`children: ${JSON.stringify(Array.from(children))}`);
  // const children = Array.from(items.keys()).map((a) => {;
                                        // return new Object({id: a, children: c == undefined ? undefined: c.map((c) => c.id)})});
  // not needed
  const children = new Map(Array.from(groupedByParent).map(([id, children]) => [id, children.map((c) => c.id)]));
  const leaves = new Map(activities.map((a) => [a.id, {...a, children: children.get(a.id)}]));
  const non_roots = new Set(Array.from(leaves.values()).filter((a) => a.children !== undefined).map((a) => a.children).flat(1));
  const roots = new Set(Array.from(leaves.keys()).filter((a) => !non_roots.has(a)));
  return {items: leaves, roots};
  
  // each item is first a root (also if it has a parent) and is only removed from roots if its parent is really found
}
