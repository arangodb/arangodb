const fs = require('fs');
const path = require('path');
const _ = require('lodash');

const findFreePort = require('@arangodb/testing/utils.js').findFreePort;
const startArango = require('@arangodb/testing/utils.js').startArango;
const makeArgsArangod = require('@arangodb/testing/utils.js').makeArgsArangod;
const executeArangod = require('@arangodb/testing/utils.js').executeArangod;
const makeAuthorizationHeaders = require('@arangodb/testing/utils.js').makeAuthorizationHeaders;
const ARANGOD_BIN = require('@arangodb/testing/utils.js').ARANGOD_BIN;
const endpointToURL = require('@arangodb/common.js').endpointToURL;

const killExternal = require('internal').killExternal;
const statusExternal = require('internal').statusExternal;
const download = require('internal').download;
const wait = require('internal').wait;

// //////////////////////////////////////////////////////////////////////////////
// / @brief periodic checks whether spawned arangod processes are still alive
// //////////////////////////////////////////////////////////////////////////////
function checkArangoAlive (arangod, options = {}) {
  if (arangod.hasOwnProperty('exitStatus')) {
    return false;
  }

  const res = statusExternal(arangod.pid, false);
  const ret = res.status === 'RUNNING';

  if (!ret) {
    print('ArangoD with PID ' + arangod.pid + ' gone:');
    print(arangod);

    if (res.hasOwnProperty('signal') &&
      ((res.signal === 11) ||
      (res.signal === 6) ||
      // Windows sometimes has random numbers in signal...
      (platform.substr(0, 3) === 'win')
      )
    ) {
      arangod.exitStatus = res;
      //analyzeServerCrash(arangod, options, 'health Check');
    }
  }

  return ret;
}

let makeArgs = function (name, rootDir, options, args) {
  args = args || options.extraArgs;

  let subDir = fs.join(rootDir, name);
  fs.makeDirectoryRecursive(subDir);

  let subArgs = makeArgsArangod(options, fs.join(subDir, 'apps'));
  subArgs = Object.assign(subArgs, args);

  return [subArgs, subDir];
};

class InstanceManager {
  constructor(name) {
    this.rootDir = fs.join(fs.getTempFile(), name);
    this.instances = [];
  }

  startDbServer(options = {}) {
    let endpoint = 'tcp://127.0.0.1:' + findFreePort(options.maxPort);
    let primaryArgs = options.extraArgs ? _.clone(options.extraArgs) : {};
    primaryArgs['server.endpoint'] = endpoint;
    primaryArgs['cluster.my-address'] = endpoint;
    primaryArgs['cluster.my-local-info'] = endpoint;
    primaryArgs['cluster.my-role'] = 'PRIMARY';
    primaryArgs['cluster.agency-endpoint'] = this.getAgencyEndpoint();
  
    this.instances.push(startArango('tcp', options, ...makeArgs('dbserver' + Math.floor(Math.random() * 1000000000), this.rootDir, options, primaryArgs), 'dbserver'));
    return this.instances[this.instances.length - 1];
  }

  getAgencyEndpoint() {
    return this.instances.filter(instance => {
      return instance.role == 'agent';
    })[0].endpoint;
  }
  
  startCoordinator(options = {}) {
    let endpoint = 'tcp://127.0.0.1:' + findFreePort(options.maxPort);
    let coordinatorArgs = options.extraArgs ? _.clone(options.extraArgs) : {};
    coordinatorArgs['server.endpoint'] = endpoint;
    coordinatorArgs['cluster.my-address'] = endpoint;
    coordinatorArgs['cluster.my-local-info'] = endpoint;
    coordinatorArgs['cluster.my-role'] = 'COORDINATOR';
    coordinatorArgs['cluster.agency-endpoint'] = this.getAgencyEndpoint();

    this.instances.push(startArango('tcp', options, ...makeArgs('coordinator' + Math.floor(Math.random() * 1000000000), this.rootDir, options, coordinatorArgs), 'coordinator'));
    return this.instances[this.instances.length - 1];
  }

  startAgency(options = {}) {
    let size = options.agencySize || 1;
    if (options.agencyWaitForSync === undefined) {
      options.agencyWaitForSync = false;
    }
    const wfs = options.agencyWaitForSync;
    for (var i=0;i<size;i++) {
      let dir = fs.join(this.rootDir, 'agency-' + i);
      fs.makeDirectoryRecursive(dir);

      let instanceArgs = {};
      instanceArgs['agency.id'] = String(i);
      instanceArgs['agency.size'] = String(size);
      instanceArgs['agency.wait-for-sync'] = String(wfs);
      instanceArgs['agency.supervision'] = 'true';
      instanceArgs['database.directory'] = path.join(dir, 'data');

      if (i === size - 1) {
        const port = findFreePort(options.maxPort);
        instanceArgs['server.endpoint'] = 'tcp://127.0.0.1:' + port;
        let l = [];
        this.instances.filter(instance => {
          return instance.role == 'agent';
        })
        .forEach(arangod => {
          l.push('--agency.endpoint');
          l.push(arangod.endpoint);
        });
        l.push('--agency.endpoint');
        l.push('tcp://127.0.0.1:' + port);
        l.push('--agency.notify');
        l.push('true');

        instanceArgs['flatCommands'] = l;
      }
      this.instances.push(startArango('tcp', options, instanceArgs, dir, 'agent'));
    }
    return this.instances.filter(instance => {
      return instance.role == 'agent';
    });
  }

  

  startCluster(numAgents, numCoordinators, numDbServers, options = {}) {
    print("Starting Cluster with Agents: " + numAgents + " Coordinators: " + numCoordinators + " DBServers: " + numDbServers);
    
    let agencyOptions = options.agents || {};
    _.extend(agencyOptions, {agencySize: numAgents});
    this.startAgency(agencyOptions);
    
    let coordinatorOptions = options.coordinators || {};
    let i;
    for (i=0;i<numCoordinators;i++) {
      this.startCoordinator(coordinatorOptions);
    }
    
    let dbServerOptions = options.dbservers || {};
    for (i=0;i<numDbServers;i++) {
      this.startDbServer(dbServerOptions);
    }

    this.waitForAllInstances();
    let debugInfo = instance => {
      print("pid: " + instance.pid +", role: " + instance.role + ", endpoint: " + instance.endpoint);
    };
    this.agents().forEach(debugInfo);
    this.coordinators().forEach(debugInfo);
    this.dbServers().forEach(debugInfo);

    return this.coordinators()[0].endpoint;
  }

  waitForAllInstances() {
    let count = 0;
    this.instances.forEach(arangod => {
      while (true) {
        const reply = download(arangod.url + '/_api/version', '', makeAuthorizationHeaders(arangod.options));

        if (!reply.error && reply.code === 200) {
          break;
        }

        ++count;

        if (count % 60 === 0) {
          if (!checkArangoAlive(arangod)) {
            throw new Error('Arangod with pid ' + arangod.pid + ' was not running. Full info: ' + JSON.stringify(arangod));
          }
        }
        wait(0.5, false);
      }
    });
    return this.getEndpoint();
  }

  getEndpoint() {
    return this.coordinators().filter(coordinator => {
      return coordinator.exitStatus && coordinator.exitStatus.status == 'RUNNING';
    })[0].endpoint;
  }

  check() {
    let failedInstances = this.instances.filter(instance => {
      instance.exitStatus = statusExternal(instance.pid, false);
      return instance.exitStatus.status != 'RUNNING';
    });

    if (failedInstances.length > 0) {
      throw new Error('Some instances died');
    }
  }

  cleanup() {
    console.warn('Shutting down cluster');
    const requestOptions = makeAuthorizationHeaders({});
    requestOptions.method = 'DELETE';

    download(endpointToURL(this.getEndpoint()) + '/_admin/shutdown?shutdown_cluster=1', '', requestOptions);
    let timeout = 60;
    let waitTime = 0.5;
    let start = Date.now();
    
    let kap0tt = false; 
    let toShutdown = this.instances.slice();
    while (toShutdown.length > 0) {
      toShutdown.forEach(instance => {
        instance.exitStatus = statusExternal(instance.pid, false);
      });

      toShutdown = toShutdown.filter(instance => {
        return instance.exitStatus.status == 'RUNNING';
      });

      if (toShutdown.length > 0) {
        let totalTime = Date.now() - start;
        if (totalTime / 1000 > timeout) {
          kap0tt = true;
          toShutdown.forEach(instance => {
            this.kill(instance);
          });
          break;
        }
        wait(waitTime);
      }
    }
    if (!kap0tt) {
      fs.removeDirectoryRecursive(this.rootDir, true);
    }
  }

  dbServers() {
    return this.instances.filter(instance => {
      return instance.role == 'dbserver';
    });
  }

  coordinators() {
    return this.instances.filter(instance => {
      return instance.role == 'coordinator';
    });
  }
  
  agents() {
    return this.instances.filter(instance => {
      return instance.role == 'agent';
    });
  }

  kill(instance) {
    let index = this.instances.indexOf(instance);
    if (index === -1) {
      throw new Error('Couldn\'t find instance', instance);
    }

    killExternal(instance.pid);
    instance.exitStatus = {'status': 'KILLED'};
  }

  restart(instance) {
    let index = this.instances.indexOf(instance);
    if (index === -1) {
      throw new Error('Couldn\'t find instance', instance);
    }

    instance.pid = executeArangod(ARANGOD_BIN, instance.args, instance.options).pid;
  }
}

module.exports = InstanceManager;
