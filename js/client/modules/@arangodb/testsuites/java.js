/* jshint strict: false, sub: true */
/* global print db */
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
  'java_driver': 'go client driver test',
};
const optionsDocumentation = [
  '   - `javasource`: directory of the go driver',
  '   - `javaOptions`: additional argumnets to pass via the commandline'
];

const internal = require('internal');

const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const statusExternal = internal.statusExternal;

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const yaml = require('js-yaml');
const platform = require('internal').platform;
const time = require('internal').time;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
// const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'java_driver': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function javaDriver (options) {
  function runInJavaTest (options, instanceInfo, file, addArgs) {
    let topology;
    let results = {};
    if (options.cluster) {
      topology = 'CLUSTER';
    } else if (options.activefailover) {
      topology = 'ACTIVE_FAILOVER';
    } else {
      topology = 'SINGLE_SERVER';
    }
    print(instanceInfo)
    let enterprise = '"false"';
    if (global.ARANGODB_CLIENT_VERSION(true).hasOwnProperty('enterprise-version')) {
      enterprise = '"true"';
    }
    db._version(true)
    let rx = /.*:\/\//gi
    print(instanceInfo.url.replace(rx, ''))
    let args = [
      'test', '-U',
      '-Dgroups="api"',
      '-Dtest.useProvidedDeployment="true"',
      '-Dtest.arangodb.version="'+ db._version() + '"',
      '-Dtest.arangodb.isEnterprise=' + enterprise,
      '-Dtest.arangodb.hosts="' + instanceInfo.url.replace(rx,'') + '"',
      '-Dtest.arangodb.authentication="root:"',
      '-Dtest.arangodb.topology="' + topology + '"'
    ];

    if (options.testCase) {
      args.push('-run');
      args.push(options.testCase);
    }
    if (options.hasOwnProperty('javaOptions')) {
      for (var key in options.javaOptions) {
        args.push('-D' + key + '="' + options.javaOptions[key] + '"');
      }
    }
    if (options.extremeVerbosity) {
      print(args);
    }
    let start = Date();
    let status = true;
    const rc = executeExternalAndWait('mvn', args, false, [], options.javasource);
    print(rc)
    if (rc.exit !== 0) {
      status = false;
    }
    results['timeout'] = false;
    results['status'] = status;
    results['message'] = '';
    return results;
  }
  runInJavaTest.info = 'runInJavaTest';

  let localOptions = _.clone(options);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }
  localOptions['server.jwt-secret'] = 'haxxmann';

  return tu.performTests(localOptions, [ 'java_test.js'], 'java_test', runInJavaTest);
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['java_driver'] = javaDriver;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
