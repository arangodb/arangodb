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
  'upgrade': 'upgrade tests'
};
const optionsDocumentation = [
];

const toArgv = require('internal').toArgv;

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const inst = require('@arangodb/testutils/instance');

const testPaths = {
  'upgrade': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: upgrade
// //////////////////////////////////////////////////////////////////////////////

function upgrade (options) {
  if (options.cluster) {
    return {
      'upgrade': {
        failed: 0,
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }

  let result = {
    upgrade: {
      failed: 0,
      status: true,
      total: 1
    }
  };

  const tmpDataDir = fs.getTempFile();
  fs.makeDirectoryRecursive(tmpDataDir);
  let instance = new inst.instance(options,
                                   inst.instanceRole.single,
                                   {'database.auto-upgrade': true},
                                   {}, 'tcp', tmpDataDir, '',
                                   new inst.agencyConfig(options, null));

  result.upgrade.first = pu.executeAndWait(pu.ARANGOD_BIN, toArgv(instance.args), options, 'upgrade', tmpDataDir, options.coreCheck);

  if (result.upgrade.first.status !== true) {
    result.upgrade.failed = 1;
    print('not removing ' + tmpDataDir);
    return result.upgrade;
  }

  ++result.upgrade.total;

  result.upgrade.second = pu.executeAndWait(pu.ARANGOD_BIN, toArgv(instance.args), options, 'upgrade', tmpDataDir, options.coreCheck);

  if (result.upgrade.second.status !== true) {
    result.upgrade.failed = 1;
    print('not removing ' + tmpDataDir);
    return result.upgrade;
  }

  result.upgrade.status = true;
  return result;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['upgrade'] = upgrade;
  defaultFns.push('upgrade');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
