/* jshint strict: false, sub: true */
/* global arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief the data fixture shared by the authorization-questions/*.js suites
// /
// / These suites are the jsunity counterpart of the standalone API tester in
// / tests/api (see tests/api/apitester.js and tests/api/apitests/*.mjs): they
// / fire the very same requests but, instead of tabulating status codes for a
// / matrix of users, they observe which authorization questions each handler
// / asks the ExecContext (see permissions-observer.js).
// /
// / The fixture mirrors the data the apitester's `setup` subcommand creates,
// / minus the permission-matrix users (the observation suites run as the single
// / connected user - normally root over basic auth - so no extra users are
// / needed):
// /
// /   database   'd'
// /   collection 'c'  - document collection, 100 documents keyed k1..k100
// /   collection 'e'  - edge collection, 100 circular edges c/k1->c/k2, ...
// /   graph      'g'  - named graph, edge definition e: c->c
// /
// / setUpApiTestData() is idempotent: it drops any leftover database 'd' first.
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;

const DB = 'd';
const DOC_COLLECTION = 'c';
const EDGE_COLLECTION = 'e';
const GRAPH = 'g';
const NUM_DOCS = 100;

// //////////////////////////////////////////////////////////////////////////////
// / @brief create database 'd' with collections c, e and graph g
// //////////////////////////////////////////////////////////////////////////////

function setUpApiTestData () {
  db._useDatabase('_system');
  try {
    db._dropDatabase(DB);
  } catch (err) {
    // database did not exist - fine
  }
  db._createDatabase(DB);
  db._useDatabase(DB);
  try {
    // document collection c with 100 documents k1..k100
    db._create(DOC_COLLECTION);
    let docs = [];
    for (let i = 1; i <= NUM_DOCS; ++i) {
      docs.push({ _key: `k${i}`, value: i });
    }
    db[DOC_COLLECTION].insert(docs);

    // edge collection e with 100 circular edges c/k1->c/k2, ..., c/k100->c/k1
    db._createEdgeCollection(EDGE_COLLECTION);
    let edges = [];
    for (let i = 1; i <= NUM_DOCS; ++i) {
      edges.push({
        _from: `${DOC_COLLECTION}/k${i}`,
        _to: `${DOC_COLLECTION}/k${(i % NUM_DOCS) + 1}`
      });
    }
    db[EDGE_COLLECTION].insert(edges);

    // named graph g with edge definition e: c->c
    const graphModule = require('@arangodb/general-graph');
    graphModule._create(GRAPH,
                        [graphModule._relation(EDGE_COLLECTION,
                                               [DOC_COLLECTION],
                                               [DOC_COLLECTION])]);
  } finally {
    db._useDatabase('_system');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief drop database 'd' (removes c, e and g)
// //////////////////////////////////////////////////////////////////////////////

function tearDownApiTestData () {
  db._useDatabase('_system');
  try {
    db._dropDatabase(DB);
  } catch (err) {
    // already gone - fine
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief questions only one of the two deployment modes asks
// /
// / Spread these into an expectation to keep the mode split visible in place:
// /
// /   assertPermissions([
// /     "UseDatabase name=d level=read",
// /     "UseCollection db=d name=c level=writedata",
// /     ...singleOnly([
// /       "UseCollection db=d name=c level=read"
// /     ])
// /   ], endObserve());
// /
// / They are pure - they must not send a request, because they are evaluated
// / while an observation is running.
// //////////////////////////////////////////////////////////////////////////////

// note: this deliberately does NOT use internal.isCluster(), which sends a
// `GET /_admin/server/role` request - the helpers below are called while an
// observation is running, and that request would show up in it. The harness
// knows the deployment layout without asking the server.
let cachedIsCluster;

function isCluster () {
  if (cachedIsCluster === undefined) {
    const { instanceRole } = require('@arangodb/testutils/instance');
    cachedIsCluster = global.instanceManager.arangods.some(
      (arangod) => arangod.isRole(instanceRole.coordinator));
  }
  return cachedIsCluster;
}

// e.g. because a single server rejects a cluster-only endpoint before asking,
// or because it resolves a collection under the caller's ExecContext where a
// coordinator does not.
function clusterOnly (questions) {
  return isCluster() ? questions : [];
}

function singleOnly (questions) {
  return isCluster() ? [] : questions;
}

exports.isCluster = isCluster;
exports.clusterOnly = clusterOnly;
exports.singleOnly = singleOnly;
exports.setUpApiTestData = setUpApiTestData;
exports.tearDownApiTestData = tearDownApiTestData;
exports.DB = DB;
exports.DOC_COLLECTION = DOC_COLLECTION;
exports.EDGE_COLLECTION = EDGE_COLLECTION;
exports.GRAPH = GRAPH;
