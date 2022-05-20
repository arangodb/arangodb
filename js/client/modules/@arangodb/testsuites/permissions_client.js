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

const _ = require('lodash');
const time = require('internal').time;
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');

const toArgv = require('internal').toArgv;
const executeScript = require('internal').executeScript;
const executeExternalAndWait = require('internal').executeExternalAndWait;

const platform = require('internal').platform;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'permissions': 'arangosh javascript access permissions'
};
const optionsDocumentation = [
  '   - `skipShebang`: if set, the shebang tests are skipped.'
];

const testPaths = {
  'permissions': [tu.pathForTesting('client/permissions')]
};
class permissionsRunner extends tu.runInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "runImport";
  }
  
  run() {
    let obj = this;
    let res = {};
    let filtered = {};
    let rootDir = fs.join(fs.getTempPath(), 'permissions');
    const tests = tu.scanTestPaths(testPaths.permissions, this.options);
    this.instanceManager = {
      rootDir: rootDir,
      endpoint: 'tcp://127.0.0.1:8888',
      findEndpoint: function() {
        return 'tcp://127.0.0.1:8888';
      },
      getStructure: function() {
        return {
          endpoint: 'tcp://127.0.0.1:8888',
          rootDir: rootDir
        };
      }
    };

    fs.makeDirectoryRecursive(rootDir);
    tests.forEach(function (f, i) {
      if (tu.filterTestcaseByOptions(f, obj.options, filtered)) {
        let t = f.split(fs.pathSeparator);
        let testName = t[t.length - 1].replace(/\.js/, '');
        let instanceRoot = fs.join(rootDir, testName);
        fs.makeDirectoryRecursive(instanceRoot);
        let testResultJson = fs.join(rootDir, 'testresult.json');;
        process.env['RESULT'] = testResultJson;
        pu.cleanupDBDirectoriesAppend(instanceRoot);

        let content = fs.read(f);
        content = `(function(){ const getOptions = true; ${content} 
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF
        let testOptions = executeScript(content, true, f);

        obj.addArgs = testOptions;
        res[f] = obj.runOneTest(/*  ,*/
          f                                
        );
      } else {
        if (obj.options.extremeVerbosity) {
          print('Skipped ' + f + ' because of ' + filtered.filter);
        }
      }

    });
    return res;
  }
}
function permissions(options) {
  return new permissionsRunner(options, "permissions").run();
}
exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['permissions'] = permissions;
  defaultFns.push('permissions');
  opts['skipShebang'] = false;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
