/* jshint strict: false, sub: true */
/* global print db */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'java_driver': 'java client driver test',
};
const optionsDocumentation = [
  '   - `javasource`: directory of the java driver',
  '   - `javaOptions`: additional arguments to pass via the commandline',
  '                    can be found in arangodb-java-driver/src/test/java/utils/TestUtils.java',
  '   - `kafkasource`: directory to contain the kafka connector',
  '   - `kafkaHost`: connection string for the possible kafka hosts',
  '   - `kafkaSchemaHost`: connection URL for the schema server',
];

const internal = require('internal');

const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const statusExternal = internal.statusExternal;

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const testRunnerBase = require('@arangodb/testutils/testrunner').testRunner;
const yaml = require('js-yaml');
const platform = require('internal').platform;
const time = require('internal').time;
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'java_driver': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

class runWithAllureReport extends testRunnerBase {
  getAllureResults(testResultsDir, results, status) {
    let allResultJsons = {};
    let topLevelContainers = [];
    let allContainerJsons = {};
    let containerRe = /-container.json/;
    let resultRe = /-result.json/;
    let resultFiles = fs.list(testResultsDir).filter(file => {
      return file.match(resultRe) !== null;
    });
    if (resultFiles.length === 0) {
      let msg = `did not find any files in ${testResultsDir}`;
      print(msg);
      results['status'] = false;
      results['message'] = msg;
    }
    //print(resultFiles)
    resultFiles.forEach(containerFile => {
      let resultJson = JSON.parse(fs.read(fs.join(testResultsDir, containerFile)));
      resultJson['parents'] = [];
      allResultJsons[resultJson.uuid] = resultJson;
    });

    let containerFiles = fs.list(testResultsDir).filter(file => file.match(containerRe) !== null);
    containerFiles.forEach(containerFile => {
      let container = JSON.parse(fs.read(fs.join(testResultsDir, containerFile)));
      container['childContainers'] = [];
      container['isToplevel'] = false;
      //print(container)
      container.children.forEach(child => {
        allResultJsons[child]['parents'].push(container.uuid);
      });
      allContainerJsons[container.uuid] = container;
    });

    for(let oneResultKey in allResultJsons) {
      allResultJsons[oneResultKey].parents = 
        allResultJsons[oneResultKey].parents.sort(function(aUuid, bUuid) {
          let a = allContainerJsons[aUuid];
          let b = allContainerJsons[bUuid];
          if ((a.start !== b.start) || (a.stop !== b.stop)) {
            return (b.stop - b.start) - (a.stop - a.start);
          }
          if (a.children.length !== b.children.length) {
            return b.children.length - a.children.length;
          }
          //print(a)
          //print(b)
          //print('--------')
          return 0;
        });
      for (let i = 0; i + 1 < allResultJsons[oneResultKey].parents.length; i++) {
        let parent = allResultJsons[oneResultKey].parents[i];
        let child = allResultJsons[oneResultKey].parents[i + 1];
        if(i === 0) {
          topLevelContainers.push(parent);
        }
        if (!allContainerJsons[parent]['childContainers'].includes(child)) {
          allContainerJsons[parent]['childContainers'].push(child);
        }
        // print(allContainerJsons[parent])
      }
      //print(allResultJsons[oneResultKey])
      //print('vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv')
    }
    //print(topLevelContainers)
    topLevelContainers.forEach(id => {
      //print('zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz')
      let tlContainer = allContainerJsons[id];
      //print(tlContainer['childContainers'])

      let resultSet = {
        duration: tlContainer.stop - tlContainer.start,
        total: tlContainer.children.length,
        failed: 0,
        status: true
      };
      results[tlContainer.name] = resultSet;

      tlContainer['childContainers'].forEach(childContainerId => {
        let childContainer = allContainerJsons[childContainerId];
        let suiteResult = {};
        resultSet[childContainer.name] = suiteResult;
        //print(childContainer);
        childContainer['childContainers'].forEach(grandChildContainerId => {
          let grandChildContainer = allContainerJsons[grandChildContainerId];
          //print(grandChildContainer);
          let name = childContainer.name + "." + grandChildContainer.name;
          if (grandChildContainer.children.length !== 1) {
            print(RED+"This grandchild has more than one item - not supported!"+RESET);
            print(RED+grandChildContainer.children+RESET);
          }
          let gcTestResult = allResultJsons[grandChildContainer.children[0]];
          let message = "";
          if (gcTestResult.hasOwnProperty('statusDetails')) {
            message = gcTestResult.statusDetails.message + "\n\n" + gcTestResult.statusDetails.trace;
          }
          let myResult = {
            duration: gcTestResult.stop - gcTestResult.start,
            status: (gcTestResult.status === "passed") || (gcTestResult.status === "skipped"),
            message: message
          };

          suiteResult[grandChildContainer.name + '.' + gcTestResult.name] = myResult;
          if (!myResult.status) {
            status = false;
            suiteResult.status = false;
            // suiteResult.message = myResult.message;
            results.message += myResult.message;
          }
        });
      });
      //print('zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz')
    });
    results['timeout'] = false;
    results['status'] = status;
  }
}

class runInJavaTest extends runWithAllureReport {
  constructor(options, testname, ...optionalArgs) {
    let opts = _.clone(tu.testClientJwtAuthInfo);
    opts['password'] = 'testjava';
    opts['username'] = 'root';
    opts['arangodConfig'] = 'arangod-auth.conf';
    _.defaults(opts, options);
    super(opts, testname, ...optionalArgs);
    this.info = "runInJavaTest";
  }
  checkSutCleannessBefore() {}
  checkSutCleannessAfter() { return true; }
  runOneTest(file) {
    print(this.instanceManager.setPassvoid());
    let topology;
    let testResultsDir = fs.join(this.instanceManager.rootDir, 'javaresults');
    let results = {
      'message': ''
    };
    let matchTopology;
    if (this.options.cluster) {
      topology = 'CLUSTER';
      matchTopology = /^CLUSTER/;
    } else {
      topology = 'SINGLE_SERVER';
      matchTopology = /^SINGLE_SERVER/;
    }

    // strip i.e. http:// from the URL to conform with what the driver expects:
    let rx = /.*:\/\//gi;
    let propertiesFileContent = `arangodb.hosts=${this.instanceManager.url.replace(rx,'')}
arangodb.password=${this.options.password}
arangodb.acquireHostList=true
`;
    let propertiesFileName = fs.join(this.options.javasource, 'test-functional/src/test/resources/arangodb.properties');
    fs.write(propertiesFileName, propertiesFileContent);
    let args = [
      'verify',
      '-am',
      '-pl',
      'test-functional',
      '-Dgpg.skip',
      '-Dmaven.javadoc.skip',
      '-Dssl=false',
      '-Dmaven.test.skip=false',
      '-DskipStatefulTests',
      '-Dallure.results.directory=' + testResultsDir,
      '-Dmaven.wagon.http.retryHandler.count=10',
      // TODO? '-Dnative=<<parameters.native>>'
    ];

    if (this.options.testCase) {
      args.push('-Dit.test=' + this.options.testCase);
      args.push('-Dfailsafe.failIfNoSpecifiedTests=false'); // if we don't specify this, errors will occur.
    }
    if (this.options.javaOptions !== '') {
      for (var key in this.options.javaOptions) {
        args.push('-D' + key + '=' + this.options.javaOptions[key]);
      }
    }
    if (this.options.extremeVerbosity) {
      print(args);
    }
    let start = Date();
    let status = true;
    const cwd = fs.normalize(fs.makeAbsolute(this.options.javasource));
    const rc = executeExternalAndWait('mvn', args, false, 0, [], cwd);
    if (rc.exit !== 0) {
      status = false;
    }
    this.getAllureResults(testResultsDir, results, status);
    return results;
  }
}

function javaDriver (options) {
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }
  let rc = new runInJavaTest(localOptions, 'java_test').run([ 'java_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


class runInKafkaTest extends runWithAllureReport {
  constructor(options, testname, ...optionalArgs) {
    let opts = _.clone(tu.testClientJwtAuthInfo);
    opts['password'] = 'test';
    opts['username'] = 'root';
    opts['jwtSecret'] = "halloJava";
    opts['arangodConfig'] = 'arangod-auth.conf';
    _.defaults(opts, options);
    super(opts, testname, ...optionalArgs);
    this.info = "runInKafkaTest";
  }
  checkSutCleannessBefore() {}
  checkSutCleannessAfter() { return true; }
  runOneTest(file) {
    print(this.instanceManager.setPassvoid());
    let topology;
    let testResultsDir = fs.join(this.instanceManager.rootDir, 'kafkaresults');
    let results = {
      'message': ''
    };

    // strip i.e. http:// from the URL to conform with what the driver expects:
    let rx = /.*:\/\//gi;
    let args = [
      'integration-test',
      `-Darango.endpoints=${this.instanceManager.url.replace(rx,'')}`,
      `-Dkafka.bootstrap.servers=${this.options.kafkaHost}`,
      '-Dgpg.skip',
      '-Dmaven.javadoc.skip',
      `-Dallure.results.directory=${testResultsDir}`,
      `-Dconnect.schema.registry.url=${this.options.kafkaSchemaHost}`,
      `-Dclient.schema.registry.url=${this.options.kafkaSchemaHost}`,
      '-Dmaven.wagon.http.retryHandler.count=10',
    ];
    if (this.options.protocol === 'ssl') {
      args.push('-Dit.test=com.arangodb.kafka.SslIT');
      args.push('-DSslTest=true');
    }

    if (this.options.testCase) {
      args.push('-Dit.test=' + this.options.testCase);
      args.push('-Dfailsafe.failIfNoSpecifiedTests=false'); // if we don't specify this, errors will occur.
    }
    if (this.options.javaOptions !== '') {
      for (var key in this.options.javaOptions) {
        args.push('-D' + key + '=' + this.options.javaOptions[key]);
      }
    }
    if (this.options.extremeVerbosity) {
      print(args);
    }
    let start = Date();
    let status = true;
    const cwd = fs.normalize(fs.makeAbsolute(this.options.kafkasource));
    const rc = executeExternalAndWait('mvn', args, false, 0, [], cwd);
    if (rc.exit !== 0) {
      status = false;
    }
    this.getAllureResults(testResultsDir, results, status);
    return results;
  }
}

function kafkaDriver (options) {
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  let rc = new runInKafkaTest(localOptions, 'kafka_test').run([ 'kafka_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


class runInSparkDatasourceTest extends runWithAllureReport {
  constructor(options, testname, ...optionalArgs) {
    let opts = _.clone(tu.testClientJwtAuthInfo);
    opts['password'] = 'test';
    opts['username'] = 'root';
    opts['jwtSecret'] = "halloJava";
    opts['arangodConfig'] = 'arangod-auth.conf';
    _.defaults(opts, options);
    super(opts, testname, ...optionalArgs);
    this.info = "runInSparkTest";
  }
  checkSutCleannessBefore() {}
  checkSutCleannessAfter() { return true; }
  runOneTest(file) {
    print(this.instanceManager.setPassvoid());
    let topology;
    let testResultsDir = fs.join(this.instanceManager.rootDir, 'sparkresults');
    let results = {
      'message': ''
    };

    // strip i.e. http:// from the URL to conform with what the driver expects:
    let rx = /.*:\/\//gi;
    let args = [
      'test',
      '-Pscala-2.12',
      '-Pspark-3.5',
      `-Darango.endpoints=${this.instanceManager.url.replace(rx,'')}`,
      `-Dallure.results.directory=${testResultsDir}`,
      '-Dmaven.wagon.http.retryHandler.count=10',
    ];

    if (this.options.testCase) {
      args.push('-Dit.test=' + this.options.testCase);
      args.push('-Dfailsafe.failIfNoSpecifiedTests=false'); // if we don't specify this, errors will occur.
    }
    if (this.options.javaOptions !== '') {
      for (var key in this.options.javaOptions) {
        args.push('-D' + key + '=' + this.options.javaOptions[key]);
      }
    }
    if (this.options.extremeVerbosity) {
      print(args);
    }
    let start = Date();
    let status = true;
    const cwd = fs.normalize(fs.makeAbsolute(this.options.sparksource));
    const rc = executeExternalAndWait('mvn', args, false, 0, [], cwd);
    if (rc.exit !== 0) {
      status = false;
    }
    this.getAllureResults(testResultsDir, results, status);
    return results;
  }
}

function sparkDataSource (options) {
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  let rc = new runInSparkDatasourceTest(localOptions, 'spark_test').run([ 'spark_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}



exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['java_driver'] = javaDriver;
  testFns['kafka_driver'] = kafkaDriver;
  testFns['spark_datasource'] = sparkDataSource;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
  tu.CopyIntoObject(opts, {
    'javaOptions': '',
    'javasource': '../arangodb-java-driver',
    'kafkasource': '../kafka-connect-arangodb',
    'kafkaHost': '172.28.0.1:19092,172.28.0.1:29092,172.28.0.1:39092',
    'kafkaSchemaHost': 'http://172.28.0.1:8081',
    'sparksource': '../arangodb-spark-datasource',
  });
};
