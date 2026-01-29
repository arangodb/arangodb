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

exports.DFS = class DFS {
  constructor(items) {
    this.items = items;
  }
  * iter(start) {
    let stack = [{hierarchy: 0, id: start}];
  
    while (stack.length > 0) {
      let {hierarchy, id} = stack.pop();
      this.items.get(id).children.forEach((c) => stack.push({hierarchy: hierarchy+1, id:c}));
      yield {hierarchy, id}
    }
  }
};

exports.Forest = class Forest {
  constructor(items) {
    this.items = items;
  }
  * iter() {

    let dfs = new exports.DFS(this.items);
    for (const root of this.roots()) {
      for (const item of dfs.iter(root)) {
        yield item;
      }
    }
  }
  roots = function() {
    const non_roots = new Set(Array.from(this.items.values()).filter((a) => a.children !== undefined).map((a) => a.children).flat(1));
    return new Set(Array.from(this.items.keys()).filter((a) => !non_roots.has(a)));
  }
};

exports.createForest = function (activities) {
  const groupedByParent = Map.groupBy(activities, (a) => a.parent); // parent_id -> [child activity, child activity]
  const children = new Map(Array.from(groupedByParent).map(([id, children]) => [id, children.map((c) => c.id)]));
  const leaves = new Map(activities.map((a) => {
    return [a.id, {...a, children: children.get(a.id) ?? []}];
  }));
  return new exports.Forest(leaves);
}
