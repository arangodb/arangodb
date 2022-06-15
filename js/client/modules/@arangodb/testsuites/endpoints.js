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
  'endpoints': 'endpoints tests'
};
const optionsDocumentation = [
  '   - `skipEndpoints`: if set to true endpoints tests are skipped',
  '   - `skipEndpointsIpv6`: if set to true endpoints tests using IPv6 are skipped',
  '   - `skipEndpointsUnix`: if set to true endpoints tests using Unix domain sockets are skipped',
];

const _ = require('lodash');
const fs = require('fs');
const internal = require('internal');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const inst = require('@arangodb/testutils/instance');
const im = require('@arangodb/testutils/instance-manager');
const sleep = internal.sleep;

const platform = require('internal').platform;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'endpoints': [tu.pathForTesting('client/endpoint-spec.js')]
};

class endpointRunner extends tu.runInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    
    this.info = "runImport";
    // we append one cleanup directory for the invoking logic...
    this.dummyDir = fs.join(fs.getTempPath(), 'endpointsdummy');
    fs.makeDirectory(this.dummyDir);
    this.instance = new inst.instance(this.options,
                                      inst.instanceRole.single,
                                      {}, {}, 'tcp', this.dummyDir, '',
                                      new inst.agencyConfig(this.options, null));
    this.endpoint = this.instance.args['server.endpoint'];
  }
  getEndpoint() {
    return this.endpoint;
  }
  run() {
    let obj = this;
    this.instanceManager = new im.instanceManager(this.options.protocol,
                                                  this.options,
                                                  this.serverOptions,
                                                  this.friendlyName);
    this.instanceManager['arangods'] = [this.instance];
    this.instanceManager.rootDir = this.instance.rootDir;
    pu.cleanupDBDirectoriesAppend(this.dummyDir);
    
    const keyFile = fs.join(tu.pathForTesting('.'), '..', '..', 'UnitTests', 'server.pem');

    let endpoints = {
      ssl: {
        skip: function () { return obj.options.skipEndpointsSSL; },
        protocol: 'ssl',
        serverArgs: function () {
          return {
            'server.endpoint': 'ssl://127.0.0.1:' + obj.instance.pm.findFreePort(obj.options.minPort, obj.options.maxPort),
            'ssl.keyfile': keyFile,
          };
        },
        shellTests: [
          {
            name: 'tcp',
            endpoint: function (endpoint) { return endpoint; },
            success: true,
            forceJson: false
          },
          { 
            name: 'any',
            endpoint: function (endpoint) { return endpoint.replace(/^ssl:\/\/.*:/, 'ssl://0.0.0.0:'); },
            success: false,
            forceJson: false
          },
          { 
            name: 'vst',
            endpoint: function (endpoint) { return endpoint.replace(/^ssl:\/\//, 'vst+ssl://'); },
            success: true,
            forceJson: false
          },
          { 
            name: 'h2',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\//, 'h2+ssl://'); },
            success: true,
            forceJson: false
          },
          { 
            name: 'non-ssl',
            endpoint: function (endpoint) { return endpoint.replace(/^ssl:\/\//, 'tcp://'); },
            success: false,
            forceJson: false
          },
        ],
      },

      tcpv4: {
        skip: function () { return obj.options.skipEndpointsIpv4; },
        protocol: 'tcp',
        serverArgs: function () {
          return {
            'server.endpoint': 'tcp://127.0.0.1:' + obj.instance.pm.findFreePort(obj.options.minPort, obj.options.maxPort)
          };
        },
        shellTests: [
          {
            name: 'tcp-json',
            endpoint: function (endpoint) { return endpoint; },
            success: true,
            forceJson: true
          },
          {
            name: 'tcp',
            endpoint: function (endpoint) { return endpoint; },
            success: true,
            forceJson: false
          },
          { 
            name: 'tcp-any',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\/.*:/, 'tcp://0.0.0.0:'); },
            success: false,
            forceJson: false
          },
          { 
            name: 'vst',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\//, 'vst://'); },
            success: true,
            forceJson: false
          },
          { 
            name: 'h2',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\//, 'h2://'); },
            success: true,
            forceJson: false
          },
          {
            name: 'ssl',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\//, 'ssl://'); },
            success: false
          },
        ],
      },

      tcpv6: {
        skip: function () { return obj.options.skipEndpointsIpv6; },
        protocol: 'tcp',
        serverArgs: function () {
          return {
            'server.endpoint': 'tcp://[::1]:' + obj.instance.pm.findFreePort(obj.options.minPort, obj.options.maxPort)
          };
        },
        shellTests: [
          {
            name: 'tcp-json',
            endpoint: function (endpoint) { return endpoint; },
            success: true,
            forceJson: false
          },
          {
            name: 'tcp',
            endpoint: function (endpoint) { return endpoint; },
            success: true,
            forceJson: false
          },
          { 
            name: 'tcp-any',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\/\[::1\]:/, 'tcp://[::]:'); },
            success: false,
            forceJson: false
          },
          { 
            name: 'vst',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\//, 'vst://'); },
            success: true,
            forceJson: false
          },
          { 
            name: 'h2',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\//, 'h2://'); },
            success: true,
            forceJson: false
          },
          {
            name: 'ssl',
            endpoint: function (endpoint) { return endpoint.replace(/^tcp:\/\//, 'ssl://'); },
            success: false,
            forceJson: false
          },
        ],
      },

      unix: {
        skip: function () { return obj.options.skipEndpointsUnix || platform.substr(0, 3) === 'win'; },
        protocol: 'unix',
        serverArgs: function () {
          // use a random filename
          return {
            'server.endpoint': 'unix://' + obj.dummyDir + '/arangodb-tmp.sock-' + require('internal').genRandomAlphaNumbers(8)
          };
        },
        shellTests: [
          {
            name: 'unix',
            endpoint: function (endpoint) { return endpoint; },
            success: true
          },
        ]
      },
    };
    obj.orgArgs = _.clone(obj.instance.args);
    return Object.keys(endpoints).reduce((results, endpointName) => {
      let testName = 'endpoint-' + endpointName;
      let testCase = endpoints[endpointName];

      if (obj.options.cluster || obj.options.skipEndpoints) {
        return {
          failed: 0,
          status: true,
          skipped: true
        };
      }
      if (testCase.skip()) {
        results[endpointName + '-' + 'all'] = {
          failed: 0,
          skipped: true,
          status: true,
          message: 'test skipped'
        };
        return results;
      }
      let serverArgs = testCase.serverArgs();
      obj.instance.args = _.defaults(serverArgs, obj.orgArgs);
      obj.instance.protocol = testCase.protocol;
      obj.instance.launchInstance({});
      if (!obj.instance.checkArangoAlive()) {
        results.failed += 1;

        results[endpointName + '-' + 'all'] = {
          failed: 1,
          status: false,
          message: 'failed to start server!'
        };
        return results;
      }
      sleep(2);
      obj.instance.checkArangoConnection(20);
      internal.env.INSTANCEINFO = JSON.stringify(obj.instance.getStructure());
      const specFile = testPaths.endpoints[0];
      let filtered = {};

      testCase.shellTests.forEach(function(testCase) {
        if (tu.filterTestcaseByOptions(testCase.name, obj.options, filtered)) {
          let old = obj.instance.endpoint;
          let shellEndpoint = testCase.endpoint(serverArgs['server.endpoint']);
          obj.endpoint = shellEndpoint;
          if (obj.options.extremeVerbosity) {
            print("Testing " + endpointName + '-' +testCase.name + " Endpoint: " + shellEndpoint);
          }
          try {
            let arangoshOpts = { 'server.connection-timeout': 2, 'server.request-timeout': 2 };
            if (testCase.forceJson) {
              arangoshOpts['server.force-json'] = true;
            }
            obj.addArgs = arangoshOpts;
            let result = obj.runOneTest(specFile);

            let success = result.status === testCase.success;
            results[endpointName + '-' + testCase.name] = { status: success }; 
            if (!success) {
              // arangosh or the test returned an error
              results[endpointName + '-' + testCase.name].message = result.message;
              results.failed += 1;
            }
          } finally {
            obj.instance.endpoint = old;
          }
        } else {
          if (obj.options.extremeVerbosity) {
            print('Skipped ' + testCase.name + ' because of ' + filtered.filter);
          }
        }
      });

      print(CYAN + 'Shutting down...' + RESET);
      let shutdown = true;
      let message = "";
      try {
        obj.instance.shutDownOneInstance({nonAgenciesCount: 1}, false, 30);
        obj.instance.waitForInstanceShutdown(10);
      } catch (ex) {
        shutdown = false;
        message = ex.message;
      }
      obj.instance.pid = null;
      obj.instance.exitStatus = null;
      print(CYAN + 'done.' + RESET);

      if (!shutdown) {
        results.failed += 1;
        results.shutdown = false;
        results.message += message;
      }
      return results;
    }, { failed: 0, shutdown: true });
    pu.cleanupLastDirectory(this.options);
  }
}

function endpoints (options) {
  print(CYAN + 'Endpoints tests...' + RESET);
  return new endpointRunner(options, "endpoints").run();
}
exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['endpoints'] = endpoints;

  opts['skipEndpoints'] = false;
  opts['skipEndpointsIpv6'] = false;
  opts['skipEndpointsIpv4'] = false;
  opts['skipEndpointsSSL'] = false;
  opts['skipEndpointsUnix'] = (platform.substr(0, 3) === 'win');

  defaultFns.push('endpoints');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
