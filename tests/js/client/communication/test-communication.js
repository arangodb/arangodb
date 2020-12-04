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

function AqlSetupPathSuite() {
  'use strict';
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));
  const twoShardColName = "UnitTestsTwoShard";

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

  const activateTriggersAQLSetupSlowPathTest = () => {
    let endpoint = endpointToURL(arango.getEndpoint());
    debugSetFailAt(endpoint, `AqlSetupSlowPath::${twoShardColName}`);
  };

  const activateTriggersAQLSetupNoSlowPathTest = () => {
    let endpoint = endpointToURL(arango.getEndpoint());
    debugSetFailAt(endpoint, `AqlSetupNoSlowPath::${twoShardColName}`);
  };

  const deactivateShardLockingFailure = () => {
    const shardList = db[twoShardColName].shards(true);
    for (const [shard, servers] of Object.entries(shardList)) {
      const endpoint = getEndpointById(servers[0]);
      debugClearFailAt(endpoint);
    }
  };

  const deactivateTriggersAQLSetupPathTest = () => {
    let endpoint = endpointToURL(arango.getEndpoint());
    debugClearFailAt(endpoint);
  };

  const docsPerWrite = 10;
  const exclusiveQuery = `db._query("FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: true}")`;
  const writeQuery = `db._query("FOR x IN 1..${docsPerWrite} INSERT {} INTO ${twoShardColName} OPTIONS {exclusive: false}")`;
  const readQuery = `db._query("FOR x IN ${twoShardColName} RETURN x")`;


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

  return {

    setUp: function () {
      db._drop(cn);
      db._create(cn);

      db._drop(twoShardColName);
      db._create(twoShardColName, { numberOfShards: 2 });
    },

    tearDown: function () {
      db._drop(cn);
      deactivateShardLockingFailure();
      deactivateTriggersAQLSetupPathTest();
      db._drop(twoShardColName);
    },

    testAqlSetupPathDeadLockExclusiveExclusive: function () {
      assertEqual(db[twoShardColName].count(), 0);
      let tests = [
        ["exclusive-1", exclusiveQuery],
        ["exclusive-2", exclusiveQuery]
      ];
      activateShardLockingFailure();
      // We enforce a deadlock -> go slowpath
      activateTriggersAQLSetupSlowPathTest();

      // run both queries in parallel
      singleRun(tests);
      assertEqual(db[twoShardColName].count(), 2 * docsPerWrite);
    },

    testAqlSetupPathDeadLockExclusiveWrite: function () {
      assertEqual(db[twoShardColName].count(), 0);
      let tests = [
        ["exclusive-1", exclusiveQuery],
        ["write-2", writeQuery]
      ];
      activateShardLockingFailure();
      // We enforce a deadlock -> go slowpath
      activateTriggersAQLSetupSlowPathTest();

      // run both queries in parallel
      singleRun(tests);
      assertEqual(db[twoShardColName].count(), 2 * docsPerWrite);
    },

    testAqlSetupPathDeadLockExclusiveRead: function () {
      assertEqual(db[twoShardColName].count(), 0);
      let tests = [
        ["exclusive-1", exclusiveQuery],
        ["read-2", readQuery]
      ];
      activateShardLockingFailure();
      // Read cannot deadlock -> do not go slow path
      activateTriggersAQLSetupNoSlowPathTest();

      // run both queries in parallel
      singleRun(tests);
      assertEqual(db[twoShardColName].count(), docsPerWrite);
    },

    testAqlSetupPathDeadLockWriteWrite: function () {
      assertEqual(db[twoShardColName].count(), 0);
      let tests = [
        ["write-1", writeQuery],
        ["write-2", writeQuery]
      ];
      activateShardLockingFailure();
      // Write / Write cannot dead lock -> do not go slow path
      activateTriggersAQLSetupNoSlowPathTest();

      // run both queries in parallel
      singleRun(tests);
      assertEqual(db[twoShardColName].count(), 2 * docsPerWrite);
    },

    testAqlSetupPathDeadLockWriteRead: function () {
      assertEqual(db[twoShardColName].count(), 0);
      let tests = [
        ["write-1", writeQuery],
        ["read-2", readQuery]
      ];
      activateShardLockingFailure();
      // Read cannot deadlock -> do not go slow path
      activateTriggersAQLSetupNoSlowPathTest();

      // run both queries in parallel
      singleRun(tests);
      assertEqual(db[twoShardColName].count(), docsPerWrite);
    },

    testAqlSetupPathDeadLockReadRead: function () {
      assertEqual(db[twoShardColName].count(), 0);
      let tests = [
        ["read-1", readQuery],
        ["read-2", readQuery]
      ];
      activateShardLockingFailure();
      // Read cannot deadlock -> do not go slow path
      activateTriggersAQLSetupNoSlowPathTest();

      // run both queries in parallel
      singleRun(tests);
      assertEqual(db[twoShardColName].count(), 0);
    },
  };
}

jsunity.run(CommunicationSuite);
if (internal.isCluster()) {
  jsunity.run(AqlSetupPathSuite);
}

return jsunity.done();
