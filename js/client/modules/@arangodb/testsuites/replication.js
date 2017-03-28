/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'replication_ongoing': 'replication ongoing tests',
  'replication_static': 'replication static tests',
  'replication_sync': 'replication sync tests'
};
const optionsDocumentation = [
];

const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_ongoing
// //////////////////////////////////////////////////////////////////////////////

function replicationOngoing (options) {
  let master = pu.startInstance('tcp', options, {}, 'master_ongoing');

  const mr = tu.makeResults('replication', master);

  if (master === false) {
    return mr(false, 'failed to start master!');
  }

  let slave = pu.startInstance('tcp', options, {}, 'slave_ongoing');

  if (slave === false) {
    pu.shutdownInstance(master, options);
    return mr(false, 'failed to start slave!');
  }

  let res = pu.run.arangoshCmd(options, master, {}, [
    '--javascript.unit-tests',
    './js/server/tests/replication/replication-ongoing.js',
    slave.endpoint
  ]);

  let results;

  if (!res.status) {
    results = mr(false, 'replication-ongoing.js failed');
  } else {
    results = mr(true);
  }

  print('Shutting down...');
  pu.shutdownInstance(slave, options);
  pu.shutdownInstance(master, options);
  print('done.');

  return results;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_static
// //////////////////////////////////////////////////////////////////////////////

function replicationStatic (options) {
  let master = pu.startInstance('tcp', options, {
    'server.authentication': 'true'
  }, 'master_static');

  const mr = tu.makeResults('replication', master);

  if (master === false) {
    return mr(false, 'failed to start master!');
  }

  let slave = pu.startInstance('tcp', options, {}, 'slave_static');

  if (slave === false) {
    pu.shutdownInstance(master, options);
    return mr(false, 'failed to start slave!');
  }

  let res = pu.run.arangoshCmd(options, master, {}, [
    '--javascript.execute-string',
    'var users = require("@arangodb/users"); ' +
    'users.save("replicator-user", "replicator-password", true); ' +
    'users.grantDatabase("replicator-user", "_system"); ' +
    'users.reload();'
  ]);

  let results;

  if (res.status) {
    res = pu.run.arangoshCmd(options, master, {}, [
      '--javascript.unit-tests',
      './js/server/tests/replication/replication-static.js',
      slave.endpoint
    ]);

    if (res.status) {
      results = mr(true);
    } else {
      results = mr(false, 'replication-static.js failed');
    }
  } else {
    results = mr(false, 'cannot create users');
  }

  print('Shutting down...');
  pu.shutdownInstance(slave, options);
  pu.shutdownInstance(master, options);
  print('done.');

  return results;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_sync
// //////////////////////////////////////////////////////////////////////////////

function replicationSync (options) {
  let master = pu.startInstance('tcp', options, {}, 'master_sync');

  const mr = tu.makeResults('replication', master);
  if (master === false) {
    return mr(false, 'failed to start master!');
  }

  let slave = pu.startInstance('tcp', options, {}, 'slave_sync');

  if (slave === false) {
    pu.shutdownInstance(master, options);
    return mr(false, 'failed to start slave!');
  }

  let res = pu.run.arangoshCmd(options, master, {}, [
    '--javascript.execute-string',
    'var users = require("@arangodb/users"); ' +
    'users.save("replicator-user", "replicator-password", true); ' +
    'users.reload();'
  ]);

  let results;

  if (res.status) {
    res = pu.run.arangoshCmd(options, master, {}, [
      '--javascript.unit-tests',
      './js/server/tests/replication/replication-sync.js',
      slave.endpoint
    ]);

    if (res.status) {
      results = mr(true);
    } else {
      results = mr(false, 'replication-sync.js failed');
    }
  } else {
    results = mr(false, 'cannot create users');
  }

  print('Shutting down...');
  pu.shutdownInstance(slave, options);
  pu.shutdownInstance(master, options);
  print('done.');

  return results;
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['replication_ongoing'] = replicationOngoing;
  testFns['replication_static'] = replicationStatic;
  testFns['replication_sync'] = replicationSync;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
