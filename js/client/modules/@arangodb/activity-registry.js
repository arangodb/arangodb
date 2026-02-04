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

exports.get_snapshot_bare = function (server) {
  if (server === undefined) {
    return arangosh.checkRequestResult(db._connection.GET(ACTIVITY_REGISTRY_URL));
  }
  IM.rememberConnection();
  IM.arangods.filter((x) => x.id === server)[0].connect();
  const result = arangosh.checkRequestResult(internal.arango.GET_RAW(ACTIVITY_REGISTRY_URL)).parsedBody;
  IM.reconnectMe();
  return result;
};

exports.get_snapshot = function (server) {
  const activities = exports.get_snapshot_bare(server);
  return exports.pretty_print(activities);
};

exports.pretty_print = function (activities) {
  const forest = exports.createForest(activities);
  return Array.from(forest.iter())
    .map(({item, hierarchy, continuations}) => `${branch_symbol(hierarchy, continuations)} ${item.name}: ${JSON.stringify(item.metadata)}`)
    .join('\n');
};

function branch_symbol(hierarchy, continuations) {
  if (hierarchy === 0) {
    return " ──";
  }
  let spacesArray = Array(4*hierarchy).fill(" ");
  Array.from(continuations).filter((c) => c < hierarchy).forEach((c) => {spacesArray[4*c] = `│`;});
  const spaces = spacesArray.join("");
  if (continuations.has(hierarchy)) {
    return spaces + `├──`;
  }
  return spaces + `└──`; 
}

exports.createForest = function (activities) {
  const groupedByParent = Map.groupBy(activities, (a) => a.parent.id);
  const children = new Map(Array.from(groupedByParent).map(([id, children]) => [id, children.map((c) => c.id)]));
  const nodes = new Map(activities.map((a) => {
    return [a.id, {...a, children: children.get(a.id) ?? []}];
  }));
  return new exports.Forest(nodes);
};

exports.DFS = class DFS {
  constructor(items) {
    this.items = items;
  }
  * iter(start) {
    let stack = [{hierarchy: 0, id: start}];
  
    while (stack.length > 0) {
      let {hierarchy, id} = stack.pop();
      const item = this.items.get(id);
      if (item === undefined) {
        return;
      }
      const continuations = new Set(stack.map(({hierarchy, id}) => hierarchy).filter((h) => h <= hierarchy));
      item.children.forEach((c) => stack.push({hierarchy: hierarchy+1, id:c}));
      // continuations are the hierarchy levels that have not yet been finished
      // because the stack includes another item of that hierarchy
      yield {item, hierarchy, continuations};
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
    const non_roots = new Set(Array.from(this.items.values()).map((a) => a.children).flat(1));
    return new Set(Array.from(this.items.keys()).filter((a) => !non_roots.has(a)));
  };
};

