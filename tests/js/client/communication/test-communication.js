/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let fs = require('fs');
let pu = require('@arangodb/testutils/process-utils');
let db = arangodb.db;
const request = require('@arangodb/request');
const graphModule = require('@arangodb/general-graph');
const { expect } = require('chai');

const getMetric = (name) => {
  let res = arango.GET_RAW("/_admin/metrics");
  let re = new RegExp("^" + name + "({[^}]*})?");
  let matches = res.body.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
  if (!matches.length) {
    throw "Metric " + name + " not found";
  }
  return Number(matches[0].replace(/^.*? (\d+.*?)$/, '$1'));
};

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

// detect the path of arangosh. quite hacky, but works
const arangosh = fs.join(global.ARANGOSH_PATH, 'arangosh' + pu.executableExt);


const debug = function (text) {
  console.warn(text);
};

function getEndpointById(id) {
  const toEndpoint = (d) => (d.endpoint);
  const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
  return instanceInfo.arangods.filter((d) => (d.id === id))
    .map(toEndpoint)
    .map(endpointToURL)[0];
}

const runShell = function (args, prefix) {
  let options = internal.options();
  args.push('--javascript.startup-directory');
  args.push(options['javascript.startup-directory']);
  for (let o in options['javascript.module-directory']) {
    args.push('--javascript.module-directory');
    args.push(options['javascript.module-directory'][o]);
  }

  let endpoint = arango.getEndpoint().replace(/\+vpp/, '').replace(/^http:/, 'tcp:').replace(/^https:/, 'ssl:').replace(/^vst:/, 'tcp:').replace(/^h2:/, 'tcp:');
  args.push('--server.endpoint');
  args.push(endpoint);
  args.push('--server.database');
  args.push(arango.getDatabaseName());
  args.push('--server.username');
  args.push(arango.connectedUser());
  args.push('--server.password');
  args.push('');
  args.push('--log.foreground-tty');
  args.push('false');
  args.push('--log.output');
  args.push('file://' + prefix + '.log');

  let result = internal.executeExternal(arangosh, args, false /*usePipes*/);
  assertTrue(result.hasOwnProperty('pid'));
  let status = internal.statusExternal(result.pid);
  assertEqual(status.status, "RUNNING");
  return result.pid;
};


function CommunicationSuite() {
  'use strict';
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));

  assertTrue(fs.isFile(arangosh), "arangosh executable not found!");



  let buildCode = function (key, command) {
    let file = fs.getTempFile() + "-" + key;
    fs.write(file, `
(function() {
  let tries = 0;
  while (true) {
    if (++tries % 10 === 0) {
      if (db['${cn}'].exists('stop')) {
        break;
      }
    }
    ${command}
  }
  db['${cn}'].insert({ _key: "${key}", done: true, iterations: tries });
})();
    `);

    let args = ['--javascript.execute', file];
    let pid = runShell(args, file);
    debug("started client with key '" + key + "', pid " + pid + ", args: " + JSON.stringify(args));
    return { key, file, pid };
  };


  let runTests = function (tests, duration) {
    assertFalse(db[cn].exists("stop"));
    let clients = [];
    debug("starting " + tests.length + " test clients");
    try {
      tests.forEach(function (test) {
        let key = test[0];
        let code = test[1];
        let client = buildCode(key, code);
        client.done = false;
        client.failed = true; // assume the worst
        clients.push(client);
      });

      debug("running test for " + duration + " s...");

      require('internal').sleep(duration);

      debug("stopping all test clients");

      // broad cast stop signal
      assertFalse(db[cn].exists("stop"));
      db[cn].insert({ _key: "stop" }, { overwriteMode: "ignore" });
      let tries = 0;
      let done = 0;
      while (++tries < 60) {
        clients.forEach(function (client) {
          if (!client.done) {
            let status = internal.statusExternal(client.pid);
            if (status.status === 'NOT-FOUND' || status.status === 'TERMINATED') {
              client.done = true;
            }
            if (status.status === 'TERMINATED' && status.exit === 0) {
              client.failed = false;
            }
          }
        });

        done = clients.reduce(function (accumulator, currentValue) {
          return accumulator + (currentValue.done ? 1 : 0);
        }, 0);

        if (done === clients.length) {
          break;
        }

        require('internal').sleep(0.5);
      }

      assertEqual(done, clients.length, "not all shells could be joined");
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
      clients.forEach(function (client) {
        try {
          fs.remove(client.file);
        } catch (err) { }

        const logfile = client.file + '.log';
        if (client.failed) {
          if (fs.exists(logfile)) {
            debug("test client with pid " + client.pid + " has failed and wrote logfile: " + fs.readFileSync(logfile).toString());
          } else {
            debug("test client with pid " + client.pid + " has failed and did not write a logfile");
          }
        }
        try {
          fs.remove(logfile);
        } catch (err) { }

        if (!client.done) {
          // hard-kill all running instances
          try {
            let status = internal.statusExternal(client.pid).status;
            if (status === 'RUNNING') {
              debug("forcefully killing test client with pid " + client.pid);
              internal.killExternal(client.pid, 9 /*SIGKILL*/);
            }
          } catch (err) { }
        }
      });
    }
  };

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

    const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
    return instanceInfo.arangods.filter(isType)
      .map(toEndpoint)
      .map(endpointToURL);
  }

  return {

    setUp: function () {
      db._drop(cn);
      db._create(cn);

      db._drop("UnitTestsTemp");
      let c = db._create("UnitTestsTemp");
      let docs = [];
      for (let i = 0; i < 50000; ++i) {
        docs.push({ value: i });
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

  /// @brief set failure point
  function debugCanUseFailAt(endpoint) {
    let res = request.get({
      url: endpoint + '/_admin/debug/failat',
    });
    return res.status === 200;
  }

  /// @brief set failure point
  function debugSetFailAt(endpoint, failAt) {
    let res = request.put({
      url: endpoint + '/_admin/debug/failat/' + failAt,
      body: ""
    });
    if (res.status !== 200) {
      throw "Error setting failure point";
    }
  }

  function debugResetRaceControl(endpoint) {
    let res = request.delete({
      url: endpoint + '/_admin/debug/raceControl',
      body: ""
    });
    if (res.status !== 200) {
      throw "Error resetting race control.";
    }
  };

  /// @brief remove failure point
  function debugRemoveFailAt(endpoint, failAt) {
    let res = request.delete({
      url: endpoint + '/_admin/debug/failat/' + failAt,
      body: ""
    });
    if (res.status !== 200) {
      throw "Error removing failure point";
    }
  }

  function debugClearFailAt(endpoint) {
    let res = request.delete({
      url: endpoint + '/_admin/debug/failat',
      body: ""
    });
    if (res.status !== 200) {
      throw "Error removing failure points";
    }
  }

  const activateShardLockingFailure = () => {
    const shardList = db[twoShardColName].shards(true);
    for (const [shard, servers] of Object.entries(shardList)) {
      const endpoint = getEndpointById(servers[0]);
      debugSetFailAt(endpoint, `WaitOnLock::${shard}`);
    }
  };


  const deactivateShardLockingFailure = () => {
    const shardList = db[twoShardColName].shards(true);
    for (const [shard, servers] of Object.entries(shardList)) {
      const endpoint = getEndpointById(servers[0]);
      debugClearFailAt(endpoint);
      debugResetRaceControl(endpoint);
    }
  };

  const docsPerWrite = 10;

  const selectExclusiveQuery = () => {
    switch (type) {
      case "Plain":
        return `db._query("FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: true}")`;
      case "Graph":
        return `db._query("FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v ${edgeName} FOR x IN 1..${docsPerWrite} INSERT {value: t._key} INTO ${twoShardColName} OPTIONS {exclusive: true}")`;
      case "NamedGraph":
        return `db._query("FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v GRAPH ${graphName} FOR x IN 1..${docsPerWrite} INSERT {value: t._key} INTO ${twoShardColName} OPTIONS {exclusive: true}")`;
      case "View":
        return `db._query("FOR v IN ${viewName} OPTIONS {waitForSync: true} FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: true}")`;
      case "Satellite":
        return `db._query("FOR v IN ${vertexName} FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: true}")`;
      default:
        // Illegal Test
        assertEqual(true, false);
    }
  };

  const selectWriteQuery = () => {
    switch (type) {
      case "Plain":
        return `db._query("FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: false}")`;
      case "Graph":
        return `db._query("FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v ${edgeName} FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: false}")`;
      case "NamedGraph":
        return `db._query("FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v GRAPH ${graphName} FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: false}")`;
      case "View":
        return `db._query("FOR v IN ${viewName} OPTIONS {waitForSync: true} FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: false}")`;
      case "Satellite":
        return `db._query("FOR v IN ${vertexName} FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: false}")`;
        default:
        // Illegal Test
        assertEqual(true, false);
    }
  };

  const selectReadQuery = () => {
    switch (type) {
      case "Plain":
        return `db._query("FOR x IN ${twoShardColName} RETURN x")`;
      case "Graph":
        return `db._query("FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v ${edgeName} FOR x IN ${twoShardColName} RETURN x")`;
      case "NamedGraph":
        return `db._query("FOR v IN ${vertexName} FOR t IN 1 OUTBOUND v GRAPH ${graphName} FOR x IN ${twoShardColName} RETURN x")`;
      case "View":
        return `db._query("FOR v IN ${viewName} OPTIONS {waitForSync: true} FOR x IN ${twoShardColName} RETURN x")`;
      case "Satellite":
        return `db._query("FOR v IN ${vertexName} FOR x IN ${twoShardColName} RETURN x")`;
      default:
        // Illegal Test
        assertEqual(true, false);
    }
  };

  const exclusiveQuery = selectExclusiveQuery();
  const writeQuery = selectWriteQuery();
  const readQuery = selectReadQuery();
  const jsWriteAction = `
    function() {
      const db = require("@arangodb").db;
      const col = db.${twoShardColName};
      for (let i = 0; i < ${docsPerWrite}; ++i) {
        col.save({});
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

  const apiLibs = `
    const request = require("@arangodb/request");
    arango.getEndpoint()
    function sendRequest(method, endpoint, body, headers) {

      const endpointToURL = (endpoint) => {
        if (endpoint.substr(0, 6) === 'ssl://') {
          return 'https://' + endpoint.substr(6);
        }
        var pos = endpoint.indexOf('://');
        if (pos === -1) {
          return 'http://' + endpoint;
        }
        return 'http' + endpoint.substr(pos);
      };

      let res;
      try {
        const envelope = {
          json: true,
          method,
          url: endpointToURL(arango.getEndpoint()) + endpoint,
          headers,
        };
        if (method !== 'GET') {
          envelope.body = body;
        }
        res = request(envelope);
      } catch (err) {
        require("console").log(err);
        return {};
      }
      if (typeof res.body === "string") {
        if (res.body === "") {
          res.body = {};
        } else {
          res.body = JSON.parse(res.body);
        }
      }
      return res;
    }
  `;
  // Note: A different test checks that the API works this way
  const apiExclusive = `
    ${apiLibs}
    let trx;
    const obj = { collections: { exclusive: "${twoShardColName}" } };
    let result = sendRequest('POST', "/_api/transaction/begin", obj, {}, true);
    try {
      trx = result.body.result.id;
      const query = "FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName}";
      sendRequest('POST', "/_api/cursor", { query }, { "x-arango-trx-id": trx });
      // Commit
      sendRequest('PUT', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
    } catch {
      // Abort
      sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
    }
  `;
  const apiWrite = `
    ${apiLibs}
    let trx;
    const obj = { collections: { write: "${twoShardColName}" } };
    let result = sendRequest('POST', "/_api/transaction/begin", obj, {});
    try {
      trx = result.body.result.id;
      const query = "FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName}";
      sendRequest('POST', "/_api/cursor", { query }, { "x-arango-trx-id": trx });
      // Commit
      sendRequest('PUT', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
    } catch {
      // Abort
      sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
    }
  `;
  const apiRead = `
    ${apiLibs}
    let trx;
    const obj = { collections: { read: "${twoShardColName}" } };
    let result = sendRequest('POST', "/_api/transaction/begin", obj, {}, true);
    try {
      trx = result.body.result.id;
      const query = "FOR x IN ${twoShardColName} RETURN x";
      sendRequest('POST', "/_api/cursor", { query }, { "x-arango-trx-id": trx });
      // Commit
      sendRequest('PUT', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
    } catch {
      // Abort
      sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
    }
  `;

  const documentWrite = `
    const docs = [];
    for (let i = 0; i < ${docsPerWrite}; ++i) {
      docs.push({});
    }
    db["${twoShardColName}"].save(docs);
  `;

  const buildSingleRunCode = (key, command) => {
    const cmd = `
    (function() {
        ${command}
        db['${cn}'].insert({ _key: "${key}", done: true, iterations: 1 });
    })();
    `;

    let args = ['--javascript.execute-string', cmd];
    let pid = runShell(args, key);
    debug("started client with key '" + key + "', pid " + pid + ", args: " + JSON.stringify(args));
    return { key, pid };
  };

  const singleRun = (tests) => {
    let clients = [];
    assertEqual(db[cn].count(), 0, "Test Reports is not empty");
    debug("starting " + tests.length + " test clients");
    try {
      tests.forEach(function ([key, code]) {
        let client = buildSingleRunCode(key, code);
        client.done = false;
        client.failed = true; // assume the worst
        clients.push(client);
      });

      debug("waiting for all test clients");

      let tries = 0;
      let done = 0;
      while (++tries < 60) {
        clients.forEach((client) => {
          if (!client.done) {
            let status = internal.statusExternal(client.pid);
            if (status.status === 'NOT-FOUND' || status.status === 'TERMINATED') {
              client.done = true;
            }
            if (status.status === 'TERMINATED' && status.exit === 0) {
              client.failed = false;
            }
          }
        });

        done = clients.reduce(function (accumulator, currentValue) {
          return accumulator + (currentValue.done ? 1 : 0);
        }, 0);

        if (done === clients.length) {
          break;
        }

        require('internal').sleep(0.5);
      }

      assertEqual(done, clients.length, "not all shells could be joined");
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
      clients.forEach(function (client) {
        const logfile = client.file + '.log';
        if (client.failed) {
          if (fs.exists(logfile)) {
            debug("test client with pid " + client.pid + " has failed and wrote logfile: " + fs.readFileSync(logfile).toString());
          } else {
            debug("test client with pid " + client.pid + " has failed and did not write a logfile");
          }
        }
        try {
          fs.remove(logfile);
        } catch (err) { }

        if (!client.done) {
          // hard-kill all running instances
          try {
            let status = internal.statusExternal(client.pid).status;
            if (status === 'RUNNING') {
              debug("forcefully killing test client with pid " + client.pid);
              internal.killExternal(client.pid, 9 /*SIGKILL*/);
            }
          } catch (err) { }
        }
      });
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
      db._create(twoShardColName, { numberOfShards: 2 });
      switch (type) {
        case "Graph":
        case "NamedGraph": {
          // We create a graph with a single vertex that has a self reference.
          const g = graphModule._create(graphName, [graphModule._relation(edgeName, vertexName, vertexName)], [], { numberOfShards: 3 });
          const v = g[vertexName].save({ _key: "a" });
          g[edgeName].save({ _from: v._id, _to: v._id });
          break;
        }
        case "View": {
          db._create(vertexName, { numberOfShards: 3 });
          db._createView(viewName, "arangosearch", { links: { [vertexName]: { includeAllFields: true } } });
          db[vertexName].save({ _key: "a" });
          break;
        }
        case "Satellite": {
          db._create(vertexName, { replicationFactor: "satellite" });
          db[vertexName].save({ _key: "a" });
          break;
        }
      }
    },

    tearDown: function () {
      deactivateShardLockingFailure();
      db[twoShardColName].truncate();
      db[cn].truncate();
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

function AqlSatelliteSetupPathSuite() {
  return GenericAqlSetupPathSuite("Satellite");
}

jsunity.run(CommunicationSuite);
if (internal.isCluster()) {
  jsunity.run(AqlSetupPathSuite);
  jsunity.run(AqlGraphSetupPathSuite);
  jsunity.run(AqlNamedGraphSetupPathSuite);
  jsunity.run(AqlViewSetupPathSuite);
  if (internal.isEnterprise()) {
    jsunity.run(AqlSatelliteSetupPathSuite);
  }
}

return jsunity.done();
