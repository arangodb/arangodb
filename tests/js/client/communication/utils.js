/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */

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
// / @author Manuel Pöter
// //////////////////////////////////////////////////////////////////////////////
const _ = require('lodash');
const internal = require('internal');
const arangodb = require('@arangodb');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const db = arangodb.db;

const toArgv = require('internal').toArgv;

'use strict';

const arangosh = fs.join(global.ARANGOSH_PATH, 'arangosh' + pu.executableExt);

const debug = function (text) {
  console.warn(text);
};

const runShell = function(args, prefix) {
  let options = internal.options();

  let endpoint = arango.getEndpoint().replace(/\+vpp/, '').replace(/^http:/, 'tcp:').replace(/^https:/, 'ssl:').replace(/^vst:/, 'tcp:').replace(/^h2:/, 'tcp:');
  let moreArgs = {
    'javascript.startup-directory': options['javascript.startup-directory'],
    'server.endpoint': endpoint,
    'server.database': arango.getDatabaseName(),
    'server.username': arango.connectedUser(),
    'server.password': '',
    'server.request-timeout': '10',
    'log.foreground-tty': 'false',
    'log.output': 'file://' + prefix + '.log'
  };
  _.assign(args, moreArgs);
  let argv = toArgv(args);

  for (let o in options['javascript.module-directory']) {
    argv.push('--javascript.module-directory');
    argv.push(options['javascript.module-directory'][o]);
  }

  let result = internal.executeExternal(arangosh, argv, false /*usePipes*/);
  assertTrue(result.hasOwnProperty('pid'));
  let status = internal.statusExternal(result.pid);
  assertEqual(status.status, "RUNNING");
  return result.pid;
};

const buildCode = function(key, command, cn) {
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

  let args = {'javascript.execute': file};
  let pid = runShell(args, file);
  debug("started client with key '" + key + "', pid " + pid + ", args: " + JSON.stringify(args));
  return { key, file, pid };
};

exports.runTests = function (tests, duration, cn) {
  assertTrue(fs.isFile(arangosh), "arangosh executable not found!");
  
  assertFalse(db[cn].exists("stop"));
  let clients = [];
  debug("starting " + tests.length + " test clients");
  try {
    tests.forEach(function (test) {
      let key = test[0];
      let code = test[1];
      let client = buildCode(key, code, cn);
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

      internal.sleep(0.5);
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
    clients.forEach(function(client) {
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
  
exports.getEndpointsByType = function(type) {
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
};
