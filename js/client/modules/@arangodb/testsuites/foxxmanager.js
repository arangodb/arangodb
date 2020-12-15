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
  'foxx_manager': 'foxx manager tests'
};
const optionsDocumentation = [
  '   - `skipFoxxQueues`: omit the test for the Foxx queues'
];

const pu = require('@arangodb/process-utils');
const fs = require('fs');

const testPaths = {
  'foxx_manager': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: foxx manager
// //////////////////////////////////////////////////////////////////////////////

function foxxManager (options) {
  print('foxx_manager tests...');

  let instanceInfo = pu.startInstance('tcp', options, {}, 'foxx_manager');

  if (instanceInfo === false) {
    return {
      foxx_manager: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  let results = {};

  results.update = pu.run.arangoshCmd(options, instanceInfo, {
    'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
  }, ['update'], options.coreCheck);

  if (results.update.status === true || options.force) {
    results.search = pu.run.arangoshCmd(options, instanceInfo, {
      'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
    }, ['search', 'itzpapalotl'], options.coreCheck);
  }

  print('Shutting down...');
  results['shutdown'] = pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return results;
}
exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['foxx_manager'] = foxxManager;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
