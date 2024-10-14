/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////
const _ = require('lodash');
let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
const isEnterprise = require("internal").isEnterprise();
let fs = require('fs');
let pu = require('@arangodb/testutils/process-utils');
let db = arangodb.db;

let { versionHas } = require('@arangodb/test-helper');

const isCov = versionHas('coverage');
const {
  launchSnippetInBG,
  joinBGShells,
  cleanupBGShells
} = require('@arangodb/testutils/client-tools').run;

const graphModule = require('@arangodb/general-graph');
const { expect } = require('chai');
const toArgv = require('internal').toArgv;


const getMetric = require('@arangodb/test-helper').getMetricSingle;
let { debugResetRaceControl } = require('@arangodb/test-helper');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const endpointToURL = (endpoint) => {
  if (endpoint.substr(0, 6) === 'ssl://') {
    return 'https://' + endpoint.substr(6);
  }
  let pos = endpoint.indexOf('://');
  if (pos === -1) {
    return 'http://' + endpoint;
  }
  return 'http' + endpoint.substr(pos);
};

const arangosh = pu.ARANGOSH_BIN;

const debug = function (text) {
  console.warn(text);
};

function getEndpointById(id) {
  const toEndpoint = (d) => (d.endpoint);
  return global.instanceManager.arangods.filter((d) => (d.id === id))
    .map(toEndpoint)
    .map(endpointToURL)[0];
}
  // TODO externalize

  function getEndpointsByType(type) {
    const isType = (d) => (d.role.toLowerCase() === type);
    const toEndpoint = (d) => (d.endpoint);
    const endpointToURL = (endpoint) => {
      if (endpoint.substr(0, 6) === 'ssl://') {
        return 'https://' + endpoint.substr(6);
      }
      let pos = endpoint.indexOf('://');
      if (pos === -1) {
        return 'http://' + endpoint;
      }
      return 'http' + endpoint.substr(pos);
    };

    return global.instanceManager.arangods.filter(isType)
      .map(toEndpoint)
      .map(endpointToURL);
  }

function CommunicationSuite() {
  'use strict';
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));

  let runTests = function (tests, duration) {
    assertFalse(db[cn].exists("stop"));
    let clients = [];
    debug("starting " + tests.length + " test clients");
    try {
      tests.forEach(function (test) {
        let key = test[0];
        let code = test[1];
        let client = launchSnippetInBG(IM.options, code, key, cn);
        client.done = false;
        client.failed = true; // assume the worst
        clients.push(client);
      });

      debug("running test for " + duration + " s...");

      internal.sleep(duration);

      debug("stopping all test clients");

      // broad cast stop signal
      assertFalse(db[cn].exists("stop"));
      db[cn].insert({ _key: "stop" }, { overwriteMode: "ignore" });
      let tries = 0;
      let done = 0;
      const waitFor = isCov ? 80 * 4 : 80;
      joinBGShells(IM.options, clients, waitFor, cn);

      assertEqual(1 + clients.length, db[cn].count());
      let stats = {};
      clients.forEach(function (client) {
        let doc = db[cn].document(client.key);
        assertEqual(client.key, doc._key);
        assertTrue(doc.done);

        stats[client.key] = doc.iterations;
      });

      debug("test run iterations: " + JSON.stringify(stats));
    } finally {
      cleanupBGShells(clients);
    }
  };


  return {

    setUp: function () {
      db._drop(cn);
      db._create(cn);

      db._drop("UnitTestsTemp");
      let c = db._create("UnitTestsTemp");
      if (isEnterprise) {
        c.ensureIndex({type: 'inverted', name: 'inverted', fields: [{ "name": "new_value", "nested": [{"name": "nested_1", "nested": ["nested_2"]}]}]});
      } else {
        c.ensureIndex({type: 'inverted', name: 'inverted', fields: [{ "name": "new_value.nested_1.nested_2"}]});
      }
      let docs = [];
      for (let i = 0; i < 50000; ++i) {
        docs.push({ value: i , new_value: [{nested_1: [{nested_2: i.toString()}]}]});
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
    },

    tearDown: function () {
      db._drop(cn);
      db._drop("UnitTestsTemp");
    },

    testWorkInParallel: function () {
      let tests = [
        ['simple-1', 'db._query("FOR doc IN _users RETURN doc");'],
        ['simple-2', 'db._query("FOR doc IN _users RETURN doc");'],
        ['insert-remove', 'db._executeTransaction({ collections: { write: "UnitTestsTemp" }, action: function() { let db = require("internal").db; let docs = []; for (let i = 0; i < 1000; ++i) docs.push({ _key: "test" + i }); let c = db.UnitTestsTemp; c.insert(docs); c.remove(docs); } });'],
        ['aql', 'db._query("FOR doc IN UnitTestsTemp RETURN doc._key");'],
      ];

      // add some cluster stuff
      if (internal.isCluster()) {
        tests.push(['cluster-health', 'if (arango.GET("/_admin/cluster/health").code !== 200) { throw "nono cluster"; }']);
      };

      // run the suite for 5 minutes
      runTests(tests, 5 * 60);
    },
  };
}

function GenericAqlSetupPathSuite(type) {
  'use strict';
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));
  const twoShardColName = "UnitTestsTwoShard";
  const graphName = "UnitTestGraph";
  const vertexName = "UnitTestVertices";
  const edgeName = "UnitTestEdges";
  const viewName = "UnitTestView";
  const searchAliasViewName = "UnitTestSearchAliasView";

  const activateShardLockingFailure = () => {
    const shardList = db[twoShardColName].shards(true);
    for (const [shard, servers] of Object.entries(shardList)) {
      IM.debugSetFailAt(`WaitOnLock::${shard}`, instanceRole.dbServer, servers[0]);
    }
  };


  const deactivateShardLockingFailure = () => {
    const shardList = db[twoShardColName].shards(true);
    for (const [shard, servers] of Object.entries(shardList)) {
      IM.debugClearFailAt(undefined, undefined, instanceRole.dbServer, servers[0]);
      IM.debugResetRaceControl(instanceRole.dbServer, servers[0]);
    }
  };

  const docsPerWrite = 10;

  const selectExclusiveQuery = () => {
    switch (type) {
      case "Plain":
        return `FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: ''}]}]} INTO ${twoShardColName} OPTIONS {exclusive: true}`;
      case "Graph":
        return `FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v ${edgeName} FOR x IN 1..${docsPerWrite} INSERT {value: t._key, b: [{c: [{d: 'a'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: true}`;
      case "NamedGraph":
        return `FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v GRAPH ${graphName} FOR x IN 1..${docsPerWrite} INSERT {value: t._key, b: [{c: [{d: 'b'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: true}`;
      case "View":
        return `FOR v IN ${viewName} OPTIONS {waitForSync: true} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'd'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: true}`;
      case "SearchAliasView":
        return `FOR v IN ${searchAliasViewName} OPTIONS {waitForSync: true} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'd'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: true}`;
      case "InvertedIndex":
        return `FOR v IN ${vertexName} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'd'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: true}`;
      case "Satellite":
        return `FOR v IN ${vertexName} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'v'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: true}`;
      default:
        // Illegal Test
        assertEqual(true, false);
    }
  };

  const selectWriteQuery = () => {
    switch (type) {
      case "Plain":
        return `FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'q'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: false}`;
      case "Graph":
        return `FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v ${edgeName} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 's'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: false}`;
      case "NamedGraph":
        return `FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v GRAPH ${graphName} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'f'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: false}`;
      case "View":
        return `FOR v IN ${viewName} OPTIONS {waitForSync: true} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'q'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: false}`;
      case "SearchAliasView":
        return `FOR v IN ${searchAliasViewName} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'q'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: false}`;
      case "InvertedIndex":
        return `FOR v IN ${vertexName} OPTIONS {waitForSync: true} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'q'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: false}`;
      case "Satellite":
        return `FOR v IN ${vertexName} FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'm'}]}]} INTO ${twoShardColName} OPTIONS {exclusive: false}`;
        default:
        // Illegal Test
        assertEqual(true, false);
    }
  };

  const selectReadQuery = () => {
    switch (type) {
      case "Plain":
        return `FOR x IN ${twoShardColName} RETURN x`;
      case "Graph":
        return `FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v ${edgeName} FOR x IN ${twoShardColName} RETURN x`;
      case "NamedGraph":
        return `FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v GRAPH ${graphName} FOR x IN ${twoShardColName} RETURN x`;
      case "View":
        return `FOR v IN ${viewName} OPTIONS {waitForSync: true} FOR x IN ${twoShardColName} RETURN x`;
      case "SearchAliasView":
        return `FOR v IN ${searchAliasViewName} OPTIONS {waitForSync: true} FOR x IN ${twoShardColName} RETURN x`;
      case "InvertedIndex":
        return `FOR v IN ${vertexName} FOR x IN ${twoShardColName} RETURN x`;  
      case "Satellite":
        return `FOR v IN ${vertexName} FOR x IN ${twoShardColName} RETURN x`;
      default:
        // Illegal Test
        assertEqual(true, false);
    }
  };

  const wrapQueryToJS = function (query) {
    return `
      console.log("starting query => ${query}");
      console.log(db._query("${query}").toArray());
     console.log("done.");
   `;
  };
  const exclusiveQuery = wrapQueryToJS(selectExclusiveQuery());
  const writeQuery = wrapQueryToJS(selectWriteQuery());
  const readQuery = wrapQueryToJS(selectReadQuery());
  const jsWriteAction = `
    function() {
      const db = require("@arangodb").db;
      const col = db.${twoShardColName};
      for (let i = 0; i < ${docsPerWrite}; ++i) {
        col.save({b: [{c: [{d: i.toString()}]}]});
        console.log("I: " + i);
      }
    }
  `;
  const jsReadAction = `
  function() {
    const db = require("@arangodb").db;
    const col = db.${twoShardColName};
    let result = col.toArray();
    return result;
  }
`;
  const jsExclusive = `
    db._executeTransaction({
      collections: {
        exclusive: "${twoShardColName}"
      },
      action: ${jsWriteAction}
    });
  `;
  const jsWrite = `
    db._executeTransaction({
      collections: {
        write: "${twoShardColName}"
      },
      action: ${jsWriteAction}
    });
  `;
  const jsRead = `
    db._executeTransaction({
      collections: {
       read: "${twoShardColName}"
      },
      action: ${jsReadAction}
    });
  `;

  // Note: A different test checks that the API works this way
  const apiExclusive = `
    let trx;
    const obj = { collections: { exclusive: "${twoShardColName}" } };
    let result = arango.POST_RAW("/_api/transaction/begin", obj);
    if (result.code === 201) {
      trx = result.parsedBody.result.id;
      const query = "FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'aaa'}]}]} INTO ${twoShardColName}";
      result = arango.POST_RAW("/_api/cursor", { query }, { "x-arango-trx-id": trx });
      if (result.code === 201) {
        // Commit
        result = arango.PUT_RAW('/_api/transaction/' + encodeURIComponent(trx), {}, {});
      }
      if (result.code !== 200) {
        console.log('apiExclusive failure:');
        console.log(result);
        console.log(arango.DELETE_RAW('/_api/transaction/' + encodeURIComponent(trx), {}, {}));
      }
    } else {
      print(result.parsedBody);
    }
  `;
  const apiWrite = `
    let trx;
    const obj = { collections: { write: "${twoShardColName}" } };
    let result = arango.POST_RAW("/_api/transaction/begin", obj, {});
    if (result.code === 201) {
      trx = result.parsedBody.result.id;
      const query = "FOR x IN 1..${docsPerWrite} INSERT {b: [{c: [{d: 'bbb'}]}]} INTO ${twoShardColName}";
      result = arango.POST_RAW("/_api/cursor", { query }, { "x-arango-trx-id": trx });
      if (result.code === 201) {
        // Commit
        result = arango.PUT_RAW('/_api/transaction/' + encodeURIComponent(trx), {}, {});
      }
      if (result.code !== 200) {
        console.log('apiWrite failure:');
        console.log(result);
        console.log(arango.DELETE_RAW('/_api/transaction/' + encodeURIComponent(trx), {}, {}));
      }
    } else {
      console.log(result.parsedBody);
    }
  `;
  const apiRead = `
    let trx;
    const obj = { collections: { read: "${twoShardColName}" } };
    let result = arango.POST_RAW("/_api/transaction/begin", obj, {});
    if (result.code === 201) {
      trx = result.parsedBody.result.id;
      const query = "FOR x IN ${twoShardColName} RETURN x";
      result = arango.POST_RAW("/_api/cursor", { query }, { "x-arango-trx-id": trx });
      if (result.code === 201) {
        // Commit
        result = arango.PUT_RAW('/_api/transaction/' + encodeURIComponent(trx), {}, {});
      }
      if (result.code !== 200) {
        console.log('apiRead failure:');
        console.log(result);
        console.log(arango.DELETE_RAW('/_api/transaction/' + encodeURIComponent(trx), {}, {}));
      }
    } else {
      console.log(result.parsedBody);
    }
  `;

  const documentWrite = `
    const docs = [];
    for (let i = 0; i < ${docsPerWrite}; ++i) {
      docs.push({b: [{c: [{d: i.toString()}]}]});
    }
    console.log("saving documents");
    db["${twoShardColName}"].save(docs);
    console.log("done");
  `;

  const singleRun = (tests) => {
    let clients = [];
    assertEqual(db[cn].count(), 0, "Test Reports is not empty");
    debug("starting " + tests.length + " test clients");
    try {
      tests.forEach(function ([key, code]) {
        let client = launchSnippetInBG(IM.options, code, key, cn, true);
        client.done = false;
        client.failed = true; // assume the worst
        clients.push(client);
      });

      debug("waiting for all test clients");
      const waitFor = isCov ? 60 * 4 : 60;
      joinBGShells(IM.options, clients, waitFor, cn);
      assertEqual(clients.length, db[cn].count());
      let stats = {};
      clients.forEach(function (client) {
        let doc = db[cn].document(client.key);
        assertEqual(client.key, doc._key);
        assertTrue(doc.done);

        stats[client.key] = doc.iterations;
      });

      debug("test run iterations: " + JSON.stringify(stats));
    } finally {
      cleanupBGShells(clients);
    }
  };

  // This is list of pairs [name, arnagosh-code, writesData]
  // We need to run all non-duplicate permutations of these pairs
  const USE_EXCLUSIVE = 1;
  const NON_EXCLUSIVE = 2;
  const NO_SHARD_SYNC = 3;

  const testCases = [
    ["Exclusive", exclusiveQuery, true, USE_EXCLUSIVE],
    ["Write", writeQuery, true, NON_EXCLUSIVE],
    ["Read", readQuery, false, NON_EXCLUSIVE],
    ["JSExclusive", jsExclusive, true, USE_EXCLUSIVE],
    ["JSWrite", jsWrite, true, NON_EXCLUSIVE],
    ["JSRead", jsRead, false, NON_EXCLUSIVE],
    ["APIExclusive", apiExclusive, true, USE_EXCLUSIVE],
    ["APIWrite", apiWrite, true, NON_EXCLUSIVE],
    ["APIRead", apiRead, false, NON_EXCLUSIVE],
    ["DocumentWrite", documentWrite, true, NO_SHARD_SYNC]
  ];

  const addTestCase = (suite, first, second) => {
    const [fName, fCode, fWrites, fExclusive] = first;
    const [sName, sCode, sWrites, sExclusive] = second;
    suite[`testAqlSetupPathDeadLock${fName}${sName}${type}`] = function () {
      assertEqual(db[twoShardColName].count(), 0);
      let tests = [
        [`${fName}-1`, fCode],
        [`${sName}-2`, sCode]
      ];
      activateShardLockingFailure();
      let numWriters = 0;
      if (fWrites) {
        numWriters++;
      }
      if (sWrites) {
        numWriters++;
      }
      const seqLocksBefore = getMetric("arangodb_collection_lock_sequential_mode");
      // run both queries in parallel
      singleRun(tests);

      const seqLocksAfter = getMetric("arangodb_collection_lock_sequential_mode");
      const expectsSequentialLock = () => {
        if (!fWrites || !sWrites) {
          // Both transactions need to write
          return false;
        }
        if (sExclusive === NO_SHARD_SYNC || fExclusive === NO_SHARD_SYNC) {
          // We do not sync shard-locks, so no deadlock possible
          return false;
        }
        // If any of the writes is exclusive we enforce sequential locking
        return fExclusive === USE_EXCLUSIVE || sExclusive === USE_EXCLUSIVE;
      };

      if (expectsSequentialLock()) {
        // Only If both tests try to write, and at least one is exclusive
        // we enforced deadlock case, and had to do sequential locking.
        assertTrue(seqLocksAfter > seqLocksBefore);
      } else {
        // Otherwise we can get away with parallel locking
        assertEqual(seqLocksAfter, seqLocksBefore);
      }
      assertEqual(db[twoShardColName].count(), numWriters * docsPerWrite);
    };
  };

  const testSuite = {
    setUpAll: function () {
      db._drop(cn);
      db._create(cn);

      db._drop(twoShardColName);
      let c = db._create(twoShardColName, { numberOfShards: 2 });
      switch (type) {
        case "Graph":
        case "NamedGraph": {
          // We create a graph with a single vertex that has a self reference.
          const g = graphModule._create(graphName, [graphModule._relation(edgeName, vertexName, vertexName)], [], { numberOfShards: 3 });
          const v = g[vertexName].save({ _key: "a", "b": [{"c": [{"d": 'bar'}]}] });
          g[edgeName].save({ _from: v._id, _to: v._id, "b": [{"c": [{"d": 'foo'}]}] });
          break;
        }
        case "View": {
          db._create(vertexName, { numberOfShards: 3 });
          let meta = {};
          if (isEnterprise) {
            meta = { links: { [vertexName]: {
              includeAllFields: true,
              fields: {
                "b": {
                  "nested": {
                    "c": {
                      "nested":{
                        "d": {}
                      }
                    }
                  }
                }
              }
            }}};
          } else {
            meta = { links: { [vertexName]: {
              includeAllFields: true
            }}};
          }
          db._createView(viewName, "arangosearch", meta);
          db[vertexName].save({ _key: "a", "b": [{"c": [{"d": 'foobar'}]}] });
          break;
        }
        case "SearchAliasView": {
          let c2 = db._create(vertexName, { numberOfShards: 3 });
          let meta = {};
          if (isEnterprise) {
            meta = {
              type: 'inverted', name: 'inverted', includeAllFields: true, fields: [{"name": "b", "nested": [{"name": "c", "nested": [{"name": "d"}]}]}]
            };
          } else {
            meta = {
              type: 'inverted', name: 'inverted', includeAllFields: true
            };
          }
          let i1 = c.ensureIndex(meta);;
          let i2 = c2.ensureIndex(meta);

          db._createView(searchAliasViewName, "search-alias", {
            indexes: [{collection: vertexName, index: i2.name}]
          });

          db[vertexName].save({ _key: "a", "b": [{"c": [{"d": 'foooof'}]}] });
          break;
        }
        case "InvertedIndex": {
          let c2 = db._create(vertexName, { numberOfShards: 3 });
          let meta = {};
          if (isEnterprise) {
            meta = {
              type: 'inverted', name: 'inverted', includeAllFields: true, fields: [{"name": "b", "nested": [{"name": "c", "nested": [{"name": "d"}]}]}]
            };
          } else {
            meta = {
              type: 'inverted', name: 'inverted', includeAllFields: true
            };
          }

          let i1 = c.ensureIndex(meta);;
          let i2 = c2.ensureIndex(meta);

          db[vertexName].save({ _key: "a", "b": [{"c": [{"d": 'foooof'}]}] });
          break;
        }
        case "Satellite": {
          db._create(vertexName, { replicationFactor: "satellite" });
          db[vertexName].save({ _key: "a", "b": [{"c": [{"d": 'foobuz'}]}] });
          break;
        }
      }
    },

    tearDown: function () {
      IM.debugClearFailAt();
      deactivateShardLockingFailure();
      db[twoShardColName].truncate({ compact: false });
      db[cn].truncate({ compact: false });
    },

    tearDownAll: function () {
      switch (type) {
        case "Graph":
        case "NamedGraph": {
          graphModule._drop(graphName, true);
          break;
        }
        case "View": {
          db._dropView(viewName);
          db._drop(vertexName);
          break;
        }
        case "SearchAliasView": {
          db._dropView(searchAliasViewName);
          db._drop(vertexName);
          break;
        }
        case "InvertedIndex": {
          db._drop(vertexName);
          break;
        }
        case "Satellite": {
          db._drop(vertexName);
          break;
        }
      }
      db._drop(twoShardColName);
      db._drop(cn);
    }
  };

  // We only need to permuate JS and API based tests for a
  // single tye, as they do not distinguish the different types
  const lastTypeTestCase = type === "Plain" ? testCases.length : 3;

  // Permutate all testCases.
  // We Iterate once over all.
  // And for each we iterate over all
  // that follow after the current case (including the current)
  // this way we get all permutations without duplicates.
  for (let i = 0; i < lastTypeTestCase; ++i) {
    for (let j = i; j < testCases.length; ++j) {
      addTestCase(testSuite, testCases[i], testCases[j]);
    }
  }

  return testSuite;
}

function AqlSetupPathSuite() {
  return GenericAqlSetupPathSuite("Plain");
}

function AqlGraphSetupPathSuite() {
  return GenericAqlSetupPathSuite("Graph");
}
function AqlNamedGraphSetupPathSuite() {
  return GenericAqlSetupPathSuite("NamedGraph");
}

function AqlViewSetupPathSuite() {
  return GenericAqlSetupPathSuite("View");
}

function AqlSearchAliasViewSetupPathSuite() {
  return GenericAqlSetupPathSuite("SearchAliasView");
}

function AqlSearchInvertedIndexSetupPathSuite() {
  return GenericAqlSetupPathSuite("InvertedIndex");
}

function AqlSatelliteSetupPathSuite() {
  return GenericAqlSetupPathSuite("Satellite");
}

// jsunity.run(CommunicationSuite);
jsunity.run(AqlSetupPathSuite);
jsunity.run(AqlGraphSetupPathSuite);
jsunity.run(AqlNamedGraphSetupPathSuite);
jsunity.run(AqlViewSetupPathSuite);
jsunity.run(AqlSearchAliasViewSetupPathSuite);
jsunity.run(AqlSearchInvertedIndexSetupPathSuite);
if (isEnterprise) {
  jsunity.run(AqlSatelliteSetupPathSuite);
}

return jsunity.done();
