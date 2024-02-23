/* jshint strict: false, sub: true */
/* global print */
'use strict';

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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'foxx_manager': 'foxx manager tests'
};
const optionsDocumentation = [
  '   - `skipFoxxQueues`: omit the test for the Foxx queues'
];

const pu = require('@arangodb/testutils/process-utils');
const im = require('@arangodb/testutils/instance-manager');
const fs = require('fs');

const testPaths = {
  'foxx_manager': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: foxx manager
// //////////////////////////////////////////////////////////////////////////////

function foxxManager (options) {
  print('foxx_manager tests...');
  let instanceManager = new im.instanceManager('tcp', options, {}, 'foxx_manager');
  instanceManager.prepareInstance();
  instanceManager.launchTcpDump("");
  if (!instanceManager.launchInstance()) {
    return {
      foxx_manager: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }
  instanceManager.reconnect();

  let results = {};

  results.update = pu.run.arangoshCmd(options, instanceManager, {
    'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
  }, ['update'], options.coreCheck);

  if (results.update.status === true || options.force) {
    results.search = pu.run.arangoshCmd(options, instanceManager, {
      'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
    }, ['search', 'itzpapalotl'], options.coreCheck);
  }

  print('Shutting down...');
  results['shutdown'] = instanceManager.shutdownInstance();
  instanceManager.destructor(results.status);
  print('done.');

  return results;
}
exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['foxx_manager'] = foxxManager;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
