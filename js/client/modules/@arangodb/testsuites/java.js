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
const { runWithAllureReport } = require('@arangodb/testutils/testrunners');
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
// / @brief TEST: java_driver
// //////////////////////////////////////////////////////////////////////////////

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
  localOptions['extraArgs']['vector-index'] =  true;
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

class runInSpringDataTest extends runWithAllureReport {
  constructor(options, testname, ...optionalArgs) {
    let opts = _.clone(tu.testClientJwtAuthInfo);
    opts['password'] = 'test';
    opts['username'] = 'root';
    opts['jwtSecret'] = "halloJava";
    opts['arangodConfig'] = 'arangod-auth.conf';
    _.defaults(opts, options);
    super(opts, testname, ...optionalArgs);
    this.info = "runInSpringDataTest";
  }
  checkSutCleannessBefore() {}
  checkSutCleannessAfter() { return true; }
  runOneTest(file) {
    print(this.instanceManager.setPassvoid());
    let topology;
    let testResultsDir = fs.join(this.instanceManager.rootDir, 'springdataresults');
    let results = {
      'message': ''
    };

    // strip i.e. http:// from the URL to conform with what the driver expects:
    let rx = /.*:\/\//gi;
    let args = [
      'test',
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
    const cwd = fs.normalize(fs.makeAbsolute(this.options.springsource));
    const rc = executeExternalAndWait('mvn', args, false, 0, [], cwd);
    if (rc.exit !== 0) {
      status = false;
    }
    this.getAllureResults(testResultsDir, results, status);
    return results;
  }
}

function springData (options) {
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  let rc = new runInSpringDataTest(localOptions, 'spring_data_test').run([ 'spring_data_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}



class runInTinkerpopProvider extends runWithAllureReport {
  constructor(options, testname, ...optionalArgs) {
    let opts = _.clone(tu.testClientJwtAuthInfo);
    opts['password'] = 'test';
    opts['username'] = 'root';
    opts['jwtSecret'] = "halloJava";
    opts['arangodConfig'] = 'arangod-auth.conf';
    _.defaults(opts, options);
    super(opts, testname, ...optionalArgs);
    this.info = "runInTinkerpopTest";
  }
  checkSutCleannessBefore() {}
  checkSutCleannessAfter() { return true; }
  runOneTest(file) {
    print(this.instanceManager.setPassvoid());
    let topology;
    let testResultsDir = fs.join(this.instanceManager.rootDir, 'tinkerpopresults');
    let results = {
      'message': ''
    };

    // strip i.e. http:// from the URL to conform with what the driver expects:
    let rx = /.*:\/\//gi;
    let args = [
      'test',
      `-Darango.endpoints=${this.instanceManager.url.replace(rx,'')}`,
      `-Dallure.results.directory=${testResultsDir}`,
      '-Dmaven.wagon.http.retryHandler.count=10',
      `-Dtest.graph.type=${this.options.tinkerpopGraphType}`,
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
    const cwd = fs.normalize(fs.makeAbsolute(this.options.tinkerpopsource));
    const rc = executeExternalAndWait('mvn', args, false, 0, [], cwd);
    if (rc.exit !== 0) {
      status = false;
    }
    this.getAllureResults(testResultsDir, results, status);
    return results;
  }
}

function tinkerpopProvider (options) {
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  let rc = new runInTinkerpopProvider(localOptions, 'tinkerpop_driver').run([ 'tinkerpop_driver.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['java_driver'] = javaDriver;
  testFns['kafka_driver'] = kafkaDriver;
  testFns['spark_datasource'] = sparkDataSource;
  testFns['spring_data'] = springData;
  testFns['tinkerpop_provider'] = tinkerpopProvider;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
  tu.CopyIntoObject(opts, {
    'javaOptions': '',
    'javasource': '../arangodb-java-driver',
    'kafkasource': '../kafka-connect-arangodb',
    'kafkaHost': '172.28.0.1:19092,172.28.0.1:29092,172.28.0.1:39092',
    'kafkaSchemaHost': 'http://172.28.0.1:8081',
    'sparksource': '../arangodb-spark-datasource',
    'springsource': '../spring-data',
    'tinkerpopsource': '../arangodb-tinkerpop-provider',
    'tinkerpopGraphType': 'simple',
  });
};
