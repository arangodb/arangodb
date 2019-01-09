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
  'replication_fuzz': 'replication randomized tests for all operations',
  'replication_random': 'replication randomized tests for transactions',
  'replication_aql': 'replication AQL tests',
  'replication_ongoing': 'replication ongoing tests',
  'replication_static': 'replication static tests',
  'replication_sync': 'replication sync tests',
  'shell_replication': 'shell replication tests'
};
const optionsDocumentation = [
];

const _ = require('lodash');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');

const testPaths = {
  'shell_replication': [tu.pathForTesting('common/replication')],
  'replication_aql': [tu.pathForTesting('server/replication')],
  'replication_fuzz': [tu.pathForTesting('server/replication')],
  'replication_random': [tu.pathForTesting('server/replication')],
  'replication_ongoing': [tu.pathForTesting('server/replication')],
  'replication_static': [tu.pathForTesting('server/replication')],
  'replication_sync': [tu.pathForTesting('server/replication')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_replication
// //////////////////////////////////////////////////////////////////////////////

function shellReplication (options) {
  let testCases = tu.scanTestPaths(testPaths.shell_replication);

  var opts = {
    'replication': true
  };
  _.defaults(opts, options);

  return tu.performTests(opts, testCases, 'shell_replication', tu.runThere);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_fuzz
// //////////////////////////////////////////////////////////////////////////////

function replicationFuzz (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_fuzz);

  options.replication = true;
  options.test = 'replication-fuzz';
  let startStopHandlers = {
    postStart: function (options,
                         serverOptions,
                         instanceInfo,
                         customInstanceInfos,
                         startStopHandlers) {
      let message;
      print("starting replication slave: ");
      let slave = pu.startInstance('tcp', options, {}, 'slave_fuzz');
      let state = (typeof slave === 'object');

      if (state) {
        message = 'failed to start slave instance!';
      }
      slave['isSlaveInstance'] = true;
      return {
        instanceInfo: slave,
        message: message,
        state: state,
        env: {
          'flatCommands': slave.endpoint
        }
      };
    },

    healthCheck: function (options,
                           serverOptions,
                           instanceInfo,
                           customInstanceInfos,
                           startStopHandlers) {
      return pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options);
    },

    preStop: function (options,
                       serverOptions,
                       instanceInfo,
                       customInstanceInfos,
                       startStopHandlers) {
      if (pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options)) {
        pu.shutdownInstance(customInstanceInfos.postStart.instanceInfo, options);
      }
      return {};
    },

    postStop: function (options,
                        serverOptions,
                        instanceInfo,
                        customInstanceInfos,
                        startStopHandlers) {
      if (options.cleanup) {
        pu.cleanupLastDirectory(options);
      }
      return { state: true };
    }

  };

  return tu.performTests(options, testCases, 'replication_fuzz', tu.runInArangosh, {}, startStopHandlers);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_random
// //////////////////////////////////////////////////////////////////////////////

function replicationRandom (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_random);

  options.replication = true;
  options.test = 'replication-random';
  let startStopHandlers = {
    postStart: function (options,
                         serverOptions,
                         instanceInfo,
                         customInstanceInfos,
                         startStopHandlers) {
      let message;
      print("starting replication slave: ");
      let slave = pu.startInstance('tcp', options, {}, 'slave_random');
      let state = (typeof slave === 'object');

      if (state) {
        message = 'failed to start slave instance!';
      }

      slave['isSlaveInstance'] = true;
      return {
        instanceInfo: slave,
        message: message,
        state: state,
        env: {
          'flatCommands': slave.endpoint
        }
      };
    },

    healthCheck: function (options,
                           serverOptions,
                           instanceInfo,
                           customInstanceInfos,
                           startStopHandlers) {
      return pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options);
    },

    preStop: function (options,
                       serverOptions,
                       instanceInfo,
                       customInstanceInfos,
                       startStopHandlers) {
      if (pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options)) {
        pu.shutdownInstance(customInstanceInfos.postStart.instanceInfo, options);
      }
      return {};
    },

    postStop: function (options,
                        serverOptions,
                        instanceInfo,
                        customInstanceInfos,
                        startStopHandlers) {
      if (options.cleanup) {
        pu.cleanupLastDirectory(options);
      }
      return { state: true };
    }

  };

  return tu.performTests(options, testCases, 'replication_random', tu.runInArangosh, {}, startStopHandlers);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_aql
// //////////////////////////////////////////////////////////////////////////////

function replicationAql (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_aql);

  options.replication = true;
  options.test = 'replication-aql';
  let startStopHandlers = {
    postStart: function (options,
                         serverOptions,
                         instanceInfo,
                         customInstanceInfos,
                         startStopHandlers) {
      let message;
      print("starting replication slave: ");
      let slave = pu.startInstance('tcp', options, {}, 'slave_aql');
      let state = (typeof slave === 'object');

      if (state) {
        message = 'failed to start slave instance!';
      }

      slave['isSlaveInstance'] = true;
      return {
        instanceInfo: slave,
        message: message,
        state: state,
        env: {
          'flatCommands': slave.endpoint
        }
      };
    },

    healthCheck: function (options,
                           serverOptions,
                           instanceInfo,
                           customInstanceInfos,
                           startStopHandlers) {
      return pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options);
    },

    preStop: function (options,
                       serverOptions,
                       instanceInfo,
                       customInstanceInfos,
                       startStopHandlers) {
      if (pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options)) {
        pu.shutdownInstance(customInstanceInfos.postStart.instanceInfo, options);
      }
      return {};
    },

    postStop: function (options,
                        serverOptions,
                        instanceInfo,
                        customInstanceInfos,
                        startStopHandlers) {
      if (options.cleanup) {
        pu.cleanupLastDirectory(options);
      }
      return { state: true };
    }

  };

  return tu.performTests(options, testCases, 'replication_aql', tu.runInArangosh, {}, startStopHandlers);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_ongoing
// //////////////////////////////////////////////////////////////////////////////

function replicationOngoing (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_ongoing);

  options.replication = true;
  if (options.test === undefined) {
    options.test = 'replication-ongoing';
  }

  let startStopHandlers = {
    postStart: function (options,
                         serverOptions,
                         instanceInfo,
                         customInstanceInfos,
                         startStopHandlers) {
      let message;
      print("starting replication slave: ");
      let slave = pu.startInstance('tcp', options, {}, 'slave_ongoing');
      let state = (typeof slave === 'object');

      if (state) {
        message = 'failed to start slave instance!';
      }

      slave['isSlaveInstance'] = true;
      return {
        instanceInfo: slave,
        message: message,
        state: state,
        env: {
          'flatCommands': slave.endpoint
        }
      };
    },

    healthCheck: function (options,
                           serverOptions,
                           instanceInfo,
                           customInstanceInfos,
                           startStopHandlers) {
      return pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options);
    },

    preStop: function (options,
                       serverOptions,
                       instanceInfo,
                       customInstanceInfos,
                       startStopHandlers) {
      if (pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options)) {
        pu.shutdownInstance(customInstanceInfos.postStart.instanceInfo, options);
      }
      return {};
    },

    postStop: function (options,
                        serverOptions,
                        instanceInfo,
                        customInstanceInfos,
                        startStopHandlers) {
      if (options.cleanup) {
        pu.cleanupLastDirectory(options);
      }
      return { state: true };
    }

  };

  return tu.performTests(options, testCases, 'replication_ongoing', tu.runInArangosh, {}, startStopHandlers);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_static
// //////////////////////////////////////////////////////////////////////////////

function replicationStatic (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_static);

  options.replication = true;
  if (options.test === undefined) {
    options.test = 'replication-static';
  }

  let startStopHandlers = {
    postStart: function (options,
                         serverOptions,
                         instanceInfo,
                         customInstanceInfos,
                         startStopHandlers) {
      let message;
      let res = true;
      print("starting replication slave: ");
      let slave = pu.startInstance('tcp', options, {}, 'slave_static');
      let state = (typeof slave === 'object');

      if (state) {
        res = pu.run.arangoshCmd(options, instanceInfo, {}, [
          '--javascript.execute-string',
          `
          var users = require("@arangodb/users");
          users.save("replicator-user", "replicator-password", true);
          users.grantDatabase("replicator-user", "_system");
          users.grantCollection("replicator-user", "_system", "*", "rw");
          users.reload();
          `
        ]);

        state = res.status;
        if (!state) {
          message = 'failed to setup slave connection' + res.message;
          pu.shutdownInstance(slave, options);
        }
        slave['isSlaveInstance'] = true;
      } else {
        message = 'failed to start slave instance!';
      }
      return {
        instanceInfo: slave,
        message: message,
        state: state,
        env: {
          'flatCommands': slave.endpoint
        }
      };
    },

    healthCheck: function (options,
                           serverOptions,
                           instanceInfo,
                           customInstanceInfos,
                           startStopHandlers) {
      return pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options);
    },

    preStop: function (options,
                       serverOptions,
                       instanceInfo,
                       customInstanceInfos,
                       startStopHandlers) {
      if (pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options)) {
        pu.shutdownInstance(customInstanceInfos.postStart.instanceInfo, options);
      }
      return {};
    },

    postStop: function (options,
                        serverOptions,
                        instanceInfo,
                        customInstanceInfos,
                        startStopHandlers) {
      if (options.cleanup) {
        pu.cleanupLastDirectory(options);
      }
      return { state: true };
    }
  };

  return tu.performTests(
    options,
    testCases,
    'master_static',
    tu.runInArangosh,
    {
      'server.authentication': 'true'
    },
    startStopHandlers);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_sync
// //////////////////////////////////////////////////////////////////////////////

function replicationSync (options) {
  let testCases = tu.scanTestPaths(testPaths.replication_sync);

  options.replication = true;
  if (options.test === undefined) {
    options.test = 'replication-sync';
  }

  let startStopHandlers = {
    postStart: function (options,
                         serverOptions,
                         instanceInfo,
                         customInstanceInfos,
                         startStopHandlers) {
      let message;
      let res = true;
      print("starting replication slave: ");
      let slave = pu.startInstance('tcp', options, {"log.level" : "replication=trace", "--log.level": "replication=trace"}, 'slave_sync');
      let state = (typeof slave === 'object');

      if (state) {
        res = pu.run.arangoshCmd(options, instanceInfo, {}, [
          '--javascript.execute-string',
          `
          var users = require("@arangodb/users");
          users.save("replicator-user", "replicator-password", true);
          users.reload();
          `
        ]);

        state = res.status;
        if (!state) {
          message = 'failed to setup slave connection' + res.message;
          pu.shutdownInstance(slave, options);
        }
        slave['isSlaveInstance'] = true;
      } else {
        message = 'failed to start slave instance!';
      }

      return {
        instanceInfo: slave,
        message: message,
        state: state,
        env: {
          'flatCommands': slave.endpoint
        }
      };
    },

    healthCheck: function (options,
                           serverOptions,
                           instanceInfo,
                           customInstanceInfos,
                           startStopHandlers) {
      return pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options);
    },

    preStop: function (options,
                       serverOptions,
                       instanceInfo,
                       customInstanceInfos,
                       startStopHandlers) {
      if (pu.arangod.check.instanceAlive(customInstanceInfos.postStart.instanceInfo, options)) {
        pu.shutdownInstance(customInstanceInfos.postStart.instanceInfo, options);
      }
      return {};
    },

    postStop: function (options,
                        serverOptions,
                        instanceInfo,
                        customInstanceInfos,
                        startStopHandlers) {
      if (options.cleanup) {
        pu.cleanupLastDirectory(options);
      }
      return { state: true };
    }
  };

  return tu.performTests(options, testCases, 'replication_sync', tu.runInArangosh, {}, startStopHandlers);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['shell_replication'] = shellReplication;
  testFns['replication_aql'] = replicationAql;
  testFns['replication_fuzz'] = replicationFuzz;
  testFns['replication_random'] = replicationRandom;
  testFns['replication_ongoing'] = replicationOngoing;
  testFns['replication_static'] = replicationStatic;
  testFns['replication_sync'] = replicationSync;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
