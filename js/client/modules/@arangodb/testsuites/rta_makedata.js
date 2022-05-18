/* jshint strict: false, sub: true */
/* global print db */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'rta_makedata': 'Release Testautomation Makedata / Checkdata framework'
};
const optionsDocumentation = [
  '   - `rtasource`: directory of the release test automation',
  '   - `makedata_args`: list of arguments ala --makedata_args:bigDoc true'
];

const internal = require('internal');

const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const statusExternal = internal.statusExternal;

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const toArgv = require('internal').toArgv;
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const im = require('@arangodb/testutils/instance-manager');
const inst = require('@arangodb/testutils/instance');
const testRunnerBase = require('@arangodb/testutils/testrunner').testRunner;
const yaml = require('js-yaml');
const platform = require('internal').platform;
const time = require('internal').time;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'rta_makedata': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function makeDataWrapper (options) {
  let stoppedDbServerInstance = {};
  class rtaMakedataRunner extends testRunnerBase {
    constructor(options, testname, ...optionalArgs) {
      super(options, testname, ...optionalArgs);
      this.info = "runRtaInArangosh";
    }
    runOneTest(file) {
      let res = {'total':0, 'duration':0.0, 'status':true};
      let tests = [
        fs.join(this.options.rtasource, 'test_data', 'makedata.js'),
        fs.join(this.options.rtasource, 'test_data', 'checkdata.js'),
        fs.join(this.options.rtasource, 'test_data', 'checkdata.js'),
        fs.join(this.options.rtasource, 'test_data', 'cleardata.js'),
      ];
      let count = 0;
      let counters = { nonAgenciesCount: 1};
      tests.forEach(file => {
        count += 1;
        let args = pu.makeArgs.arangosh(this.options);
        args['server.endpoint'] = this.instanceManager.findEndpoint();
        args['javascript.execute'] = file;
        if (this.options.forceJson) {
          args['server.force-json'] = true;
        }
        if (!this.options.verbose) {
          args['log.level'] = 'warning';
        }
        if (this.addArgs !== undefined) {
          args = Object.assign(args, this.addArgs);
        }
        let argv = toArgv(args);
        argv = argv.concat(['--',
                            '--minReplicationFactor', '2',
                            '--progress', 'true',
                            '--oldVersion', db._version()
                           ]);
        if (this.options.hasOwnProperty('makedata_args')) {
          argv = argv.concat(toArgv(this.options['makedata_args']));
        }
        if ((this.options.cluster) && (count === 3)) {
          this.instanceManager.arangods.forEach(function (oneInstance, i) {
            if (oneInstance.isRole(inst.instanceRole.dbServer)) {
              stoppedDbServerInstance = oneInstance;
            }
          });
          print('stopping dbserver ' + stoppedDbServerInstance.name +
                ' ID: ' + stoppedDbServerInstance.id +JSON.stringify( stoppedDbServerInstance.getStructure()));
          stoppedDbServerInstance.shutDownOneInstance(counters, false, 10);
          stoppedDbServerInstance.waitForExit();
          argv = argv.concat([ '--disabledDbserverUUID', stoppedDbServerInstance.id]);
        }
        require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager.getStructure());
        if (this.options.extremeVerbosity !== 'silence') {
          print(argv);
        }
        let rc = pu.executeAndWait(pu.ARANGOSH_BIN, argv, this.options, 'arangosh', this.instanceManager.rootDir, this.options.coreCheck);
        res.total++;
        res.duration += rc.duration;
        res.status &= rc.status;

        if ((this.options.cluster) && (count === 3)) {
          print('relaunching dbserver');
          stoppedDbServerInstance.restartOneInstance({});
          
        }
      });
      return res;
    }
  }
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  let rc = new rtaMakedataRunner(localOptions, 'rta_makedata_test').run(['rta']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['rta_makedata'] = makeDataWrapper;
  opts['rtasource'] = fs.makeAbsolute(fs.join('.', '..','release-test-automation'));
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
