/* jshint strict: false, sub: true */
/* global print, arango */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const rp = require('@arangodb/testutils/result-processing');
const yaml = require('js-yaml');
const internal = require('internal');
const crashUtils = require('@arangodb/testutils/crash-utils');
const crypto = require('@arangodb/crypto');
const ArangoError = require('@arangodb').ArangoError;
const debugGetFailurePoints = require('@arangodb/test-helper').debugGetFailurePoints;

/* Functions: */
const toArgv = internal.toArgv;
const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const killExternal = internal.killExternal;
const statusExternal = internal.statusExternal;
const statisticsExternal = internal.statisticsExternal;
const base64Encode = internal.base64Encode;
const testPort = internal.testPort;
const download = internal.download;
const time = internal.time;
const wait = internal.wait;
const sleep = internal.sleep;

/* Constants: */
// const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = internal.COLORS.COLOR_YELLOW;
const IS_A_TTY = RED.length !== 0;

const platform = internal.platform;

const abortSignal = 6;
const termSignal = 15;

let tcpdump;
let assertLines = [];

let PORTMANAGER;
class portManager {
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief finds a free port
  // //////////////////////////////////////////////////////////////////////////////
  constructor(options) {
    this.usedPorts = [];
    this.minPort = options['minPort'];
    this.maxPort = options['maxPort'];
    if (typeof this.maxPort !== 'number') {
      this.maxPort = 32768;
    }

    if (this.maxPort - this.minPort < 0) {
      throw new Error('minPort ' + this.minPort + ' is smaller than maxPort ' + this.maxPort);
    }
  }
  findFreePort() {
    let tries = 0;
    while (true) {
      const port = Math.floor(Math.random() * (this.maxPort - this.minPort)) + this.minPort;
      tries++;
      if (tries > 20) {
        throw new Error('Couldn\'t find a port after ' + tries + ' tries. portrange of ' + this.minPort + ', ' + this.maxPort + ' too narrow?');
      }
      if (this.usedPorts.indexOf(port) >= 0) {
        continue;
      }
      const free = testPort('tcp://0.0.0.0:' + port);

      if (free) {
        this.usedPorts.push(port);
        return port;
      }

      internal.wait(0.1, false);
    }
  }
}

const instanceType = {
  single: 'single',
  activefailover : 'activefailover',
  cluster: 'cluster'
};

const instanceRole = {
  single: 'single',
  agent: 'agent',
  dbServer: 'dbserver',
  coordinator: 'coordinator',
  failover: 'activefailover'
};

class agencyConfig {
  constructor(options) {
    this.agencySize = options.agencySize;
    this.supervision = String(options.agencySupervision);
    this.waitForSync = false;
    if (options.agencyWaitForSync !== undefined) {
      this.waitForSync = options.agencyWaitForSync = false;
    }
    this.agencyInstances = [];
    this.agencyEndpoint = "";
    this.agentsLaunched = 0;
  }
}

class oneInstance {
  // / protocol must be one of ["tcp", "ssl", "unix"]
  constructor(options, instanceRole, addArgs, authHeaders, protocol, rootDir, restKeyFile, agencyConfig) {
    this.options = options;
    this.instanceRole = instanceRole;
    this.rootDir = rootDir;
    this.protocol = protocol;
    this.args = _.clone(addArgs);
    this.authHeaders = authHeaders;
    this.restKeyFile = restKeyFile;
    this.agencyConfig = agencyConfig;

    this.message = '';
    this.arangods = [];
    this.port = null;
    this.endpoint = null;

    this.dataDir = fs.join(this.rootDir, 'data');
    this.appDir = fs.join(this.rootDir, 'apps');
    this.tmpDir = fs.join(this.rootDir, 'tmp');

    fs.makeDirectoryRecursive(this.dataDir);
    fs.makeDirectoryRecursive(this.appDir);
    fs.makeDirectoryRecursive(this.tmpDir);
    this.logFile = fs.join(rootDir, 'log');
    this._makeArgsArangod();
    
    this.name = instanceRole.name + ' - ' + this.port;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief arguments for testing (server)
  // //////////////////////////////////////////////////////////////////////////////

  _makeArgsArangod () {
    console.assert(this.tmpDir !== undefined);
    let endpoint;

    if (!this.args.hasOwnProperty('server.endpoint')) {
      this.port = PORTMANAGER.findFreePort(this.options.minPort, this.options.maxPort);
      this.endpoint = this.protocol + '://127.0.0.1:' + this.port;
    } else {
      this.endpoint = this.args['server.endpoint'];
      this.port = this.endpoint.split(':').pop();
    }

    if (this.appDir === undefined) {
      this.appDir = fs.getTempPath();
    }

    let config = 'arangod.conf';

    if (this.instanceType !== instanceRole.single) {
      config = 'arangod-' + this.instanceRole + '.conf';
    }

    this.args = _.defaults(this.args, {
      'configuration': fs.join(pu.CONFIG_DIR, config),
      'define': 'TOP_DIR=' + pu.TOP_DIR,
      'javascript.app-path': this.appDir,
      'javascript.copy-installation': false,
      'http.trusted-origin': this.options.httpTrustedOrigin || 'all',
      'temp.path': this.tmpDir,
      'server.endpoint': this.endpoint,
      'database.directory': this.dataDir,
      'log.file': this.logFile
    });
    if (this.options.auditLoggingEnabled) {
      this.args['audit.output'] = 'file://' + fs.join(this.rootDir, 'audit.log');
      this.args['server.statistics'] = false;
      this.args['foxx.queues'] = false;
    }

    if (this.protocol === 'ssl') {
      this.args['ssl.keyfile'] = fs.join('UnitTests', 'server.pem');
    }
    if (this.options.encryptionAtRest) {
      this.args['rocksdb.encryption-keyfile'] = this.restKeyFile;
    }
    if (this.restKeyFile) {
      this.args['server.jwt-secret'] = this.restKeyFile;
    }
    //TODO server_secrets else if (addArgs['server.jwt-secret-folder'] && !instanceInfo.authOpts['server.jwt-secret-folder']) {
    // instanceInfo.authOpts['server.jwt-secret-folder'] = addArgs['server.jwt-secret-folder'];
    //}
    this.args = Object.assign(this.args, this.options.extraArgs);

    if (this.options.verbose) {
      this.args['log.level'] = 'debug';
    } else if (this.options.noStartStopLogs) {
      this.args['log.level'] = 'all=error';
    }
    if (this.instanceRole === instanceRole.agent) {
      this.args = Object.assign(this.args, {
        'agency.activate': 'true',
        'agency.size': this.agencyConfig.agencySize,
        'agency.pool-size': this.agencyConfig.agencySize,
        'agency.wait-for-sync': this.agencyConfig.waitForSync,
        'agency.supervision': this.agencyConfig.supervision,
        'agency.my-address': this.protocol + '://127.0.0.1:' + this.port
      });
      if (!this.args.hasOwnProperty("agency.supervision-grace-period")) {
        this.args['agency.supervision-grace-period'] = '10.0';
      }
      if (!this.args.hasOwnProperty("agency.supervision-frequency")) {
        this.args['agency.supervision-frequency'] = '1.0';
      }
      this.agencyConfig.agencyInstances.push(this);
      if (this.agencyConfig.agencyInstances.length === this.agencyConfig.agencySize) {
        let l = [];
        this.agencyConfig.agencyInstances.forEach(agentInstance => {
          l.push(agentInstance.endpoint);
        });
        this.agencyConfig.agencyInstances.forEach(agentInstance => {
          agentInstance.args['agency.endpoint'] = _.clone(l);
        });
        this.agencyConfig.agencyEndpoint = this.agencyConfig.agencyInstances[0].endpoint;
      }
    } else if (this.instanceRole === instanceRole.dbServer) {
      this.args = Object.assign(this.args, {
        'cluster.my-role':'PRIMARY',
        'cluster.my-address': this.args['server.endpoint'],
        'cluster.agency-endpoint': this.agencyConfig.agencyEndpoint
      });
    } else if (this.instanceRole === instanceRole.coordinator) {
      this.args = Object.assign(this.args, {
        'cluster.my-role': 'COORDINATOR',
        'cluster.my-address': this.args['server.endpoint'],
        'cluster.agency-endpoint': this.agencyConfig.agencyEndpoint,
        'foxx.force-update-on-startup': true
      });
      if (!this.args.hasOwnProperty('cluster.default-replication-factor')) {
        this.args['cluster.default-replication-factor'] = (platform.substr(0, 3) === 'win') ? '1':'2';
      }
    } else if (this.instanceRole === instanceRole.failover) {
      this.args = Object.assign(this.args, {
        'cluster.my-role': 'SINGLE',
        'cluster.my-address': this.args['server.endpoint'],
        'cluster.agency-endpoint': this.agencyConfig.agencyEndpoint,
        'replication.active-failover': true
      });
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief aggregates information from /proc about the SUT
  // //////////////////////////////////////////////////////////////////////////////
  _getProcessStats() {
    let processStats = statisticsExternal(this.pid);
    if (platform === 'linux') {
      let pidStr = "" + this.pid;
      let ioraw;
      let fn = fs.join('/', 'proc', pidStr, 'io');
      try {
        ioraw = fs.readBuffer(fn);
      } catch (x) {
        print("Proc FN gone: " + fn);
        print(x.stack);
        throw x;
      }
      /*
       * rchar: 1409391
       * wchar: 681539
       * syscr: 3303
       * syscw: 2969
       * read_bytes: 0
       * write_bytes: 0
       * cancelled_write_bytes: 0
       */
      let lineStart = 0;
      let maxBuffer = ioraw.length;
      for (let j = 0; j < maxBuffer; j++) {
        if (ioraw[j] === 10) { // \n
          const line = ioraw.asciiSlice(lineStart, j);
          lineStart = j + 1;
          let x = line.split(":");
          processStats[x[0]] = parseInt(x[1]);
        }
      }
      /* 
       * sockets: used 1272
       * TCP: inuse 27 orphan 0 tw 117 alloc 382 mem 25
       * UDP: inuse 19 mem 17
       * UDPLITE: inuse 0
       * RAW: inuse 0
       * FRAG: inuse 0 memory 0
       */
      ioraw = getSockStatFile(this.pid);
      ioraw.split('\n').forEach(line => {
        if (line.length > 0) {
          let x = line.split(":");
          let values = x[1].split(" ");
          for (let k = 1; k < values.length; k+= 2) {
            processStats['sockstat_' + x[0] + '_' + values[k]]
              = parseInt(values[k + 1]);
          }
        }
      });
    }
    return processStats;
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief scans the log files for assert lines
  // //////////////////////////////////////////////////////////////////////////////

  readAssertLogLines () {
    const buf = fs.readBuffer(this.logFile);
    let lineStart = 0;
    let maxBuffer = buf.length;

    for (let j = 0; j < maxBuffer; j++) {
      if (buf[j] === 10) { // \n
        const line = buf.asciiSlice(lineStart, j);
        lineStart = j + 1;
        
        // scan for asserts from the crash dumper
        if (line.search('{crash}') !== -1) {
          if (!IS_A_TTY) {
            // else the server has already printed these:
            print("ERROR: " + line);
          }
          assertLines.push(line);
        }
      }
    }
  }
  terminateInstance() {
    if (!this.hasOwnProperty('exitStatus')) {
      // killWithCoreDump(options, arangod);
      this.exitStatus = killExternal(this.pid, termSignal);
    }
  }

  readImportantLogLines (logPath) {
    let fnLines = [];
    const buf = fs.readBuffer(fs.join(this.logFile));
    let lineStart = 0;
    let maxBuffer = buf.length;
    
    for (let j = 0; j < maxBuffer; j++) {
      if (buf[j] === 10) { // \n
        const line = buf.asciiSlice(lineStart, j);
        lineStart = j + 1;
        
        // filter out regular INFO lines, and test related messages
        let warn = line.search('WARNING about to execute:') !== -1;
        let info = line.search(' INFO ') !== -1;
        
        if (warn || info) {
          continue;
        }
        fnLines.push(line);
      }
    }
    return fnLines;
  }
  aggregateFatalErrors(currentTest) {
    if (assertLines.length > 0) {
      assertLines.forEach(line => {
        rp.addFailRunsMessage(currentTest, line);
      });
      assertLines = [];
    }
    if (serverCrashedLocal) {
      rp.addFailRunsMessage(currentTest, serverFailMessagesLocal);
      serverFailMessagesLocal = "";
    }
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief executes a command, possible with valgrind
  // //////////////////////////////////////////////////////////////////////////////

  executeArangod () {
    let cmd = pu.ARANGOD_BIN;
    let args = [];
    if (this.options.valgrind) {
      let valgrindOpts = {};

      if (this.options.valgrindArgs) {
        valgrindOpts = this.options.valgrindArgs;
      }

      let testfn = this.options.valgrindFileBase;

      if (testfn.length > 0) {
        testfn += '_';
      }

      if (valgrindOpts.xml === 'yes') {
        valgrindOpts['xml-file'] = testfn + '.%p.xml';
      }

      valgrindOpts['log-file'] = testfn + '.%p.valgrind.log';

      args = toArgv(valgrindOpts, true).concat([cmd]).concat(toArgv(this.args));
      cmd = this.options.valgrind;
    } else if (this.options.rr) {
      args = [cmd].concat(args);
      cmd = 'rr';
    } else {
      args = toArgv(this.args);
    }

    if (this.options.extremeVerbosity) {
      print(Date() + ' starting process ' + cmd + ' with arguments: ' + JSON.stringify(args));
    }

    return executeExternal(cmd, args, false, pu.coverageEnvironment());
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief starts an instance
  // /
  // //////////////////////////////////////////////////////////////////////////////

  startArango () {
    this.url = pu.endpointToURL(this.endpoint);

    try {
      this.pid = this.executeArangod().pid;
    } catch (x) {
      print(Date() + ' failed to run arangod - ' + JSON.stringify(x));

      throw x;
    }

    if (crashUtils.isEnabledWindowsMonitor(this.options, this, this.pid, pu.ARANGOD_BIN)) {
      if (!crashUtils.runProcdump(this.options, this, this.rootDir, this.pid)) {
        print('Killing ' + pu.ARANGOD_BIN + ' - ' + JSON.stringify(this.args));
        let res = killExternal(this.pid);
        this.pid = res.pid;
        this.exitStatus = res;
        throw new Error("launching procdump failed, aborting.");
      }
    }
    sleep(0.5);
    if (this.instanceRole === instanceRole.agent) {
      this.agencyConfig.agentsLaunched += 1;
      if (this.agencyConfig.agentsLaunched === this.agencyConfig.agencySize) {
        this.agencyConfig.wholeInstance.checkClusterAlive()
      }
    }
  }
  launchInstance(options, oneInstanceInfo, moreArgs) {
    if (oneInstanceInfo.pid) {
      return;
    }
    try {
      Object.assign(oneInstanceInfo.args, moreArgs);
      /// TODO Y? Where?
      oneInstanceInfo.pid = this.executeArangod(oneInstanceInfo.args).pid;
    } catch (x) {
      print(Date() + ' failed to run arangod - ' + JSON.stringify(x));

      throw x;
    }
    if (crashUtils.isEnabledWindowsMonitor(this.options, oneInstanceInfo, oneInstanceInfo.pid, pu.ARANGOD_BIN)) {
      if (!crashUtils.runProcdump(this.options, oneInstanceInfo, oneInstanceInfo.rootDir, oneInstanceInfo.pid)) {
        print('Killing ' + pu.ARANGOD_BIN + ' - ' + JSON.stringify(oneInstanceInfo.args));
        let res = killExternal(oneInstanceInfo.pid);
        oneInstanceInfo.pid = res.pid;
        oneInstanceInfo.exitStatus = res;
        throw new Error("launching procdump failed, aborting.");
      }
    }
  };

  restartOneInstance(moreArgs) {
    const startTime = time();
    delete(this.exitStatus);
    delete(this.pid);
    this.upAndRunning = false;

    print("relaunching: " + JSON.stringify(this));
    this.launchInstance(moreArgs);
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief periodic checks whether spawned arangod processes are still alive
  // //////////////////////////////////////////////////////////////////////////////
  checkArangoAlive () {
    const res = statusExternal(this.pid, false);
    const ret = res.status === 'RUNNING' && crashUtils.checkMonitorAlive(pu.ARANGOD_BIN, this, this.options, res);

    if (!ret) {
      if (!this.hasOwnProperty('message')) {
        this.message = '';
      }
      let msg = ' ArangoD of role [' + this.name + '] with PID ' + this.pid + ' is gone';
      print(Date() + msg + ':');
      this.message += (this.message.length === 0) ? '\n' : '' + msg + ' ';
      if (!this.hasOwnProperty('exitStatus')) {
        this.exitStatus = res;
      }
      print(this);

      if (res.hasOwnProperty('signal') &&
          ((res.signal === 11) ||
           (res.signal === 6) ||
           // Windows sometimes has random numbers in signal...
           (platform.substr(0, 3) === 'win')
          )
         ) {
        this.exitStatus = res;
        msg = 'health Check Signal(' + res.signal + ') ';
        analyzeServerCrash(this, this.options, msg);
        serverCrashedLocal = true;
        this.message += msg;
        msg = " checkArangoAlive: Marking crashy";
        this.message += msg;
        print(Date() + msg + ' - ' + JSON.stringify(this));
      }
    }
    return ret;
  }
  dumpAgent(path, method, fn) {
    let opts = {
      method: method
    };
    if (this.args.hasOwnProperty('authOpts')) {
      opts['jwt'] = crypto.jwtEncode(this.authOpts['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
    }
    print('--------------------------------- '+ fn + ' -----------------------------------------------');
    let agencyReply = download(this.url + path, method === 'POST' ? '[["/"]]' : '', opts);
    if (agencyReply.code === 200) {
      let agencyValue = JSON.parse(agencyReply.body);
      fs.write(fs.join(this.options.testOutputDirectory, fn + '_' + this.pid + ".json"), JSON.stringify(agencyValue, null, 2));
    } else {
      print(agencyReply);
    }
  }
}

class instanceManager {
  constructor(protocol, options, addArgs, testname, tmpDir) {
    this.protocol = protocol;
    this.options = options;
    this.addArgs = addArgs;
    this.rootDir = fs.join(tmpDir || fs.getTempPath(), testname);
    this.options.agency = this.options.agency || this.options.cluster || this.options.activefailover;
    if (! PORTMANAGER) {
      PORTMANAGER = new portManager(options);
    }
    this.agencyConfig = new agencyConfig(options);
    this.httpAuthOptions = {};
    this.arangods = [];
    this.restKeyFile = null;
    if (this.options.encryptionAtRest) {
      this.restKeyFile = fs.join(this.rootDir, 'openSesame.txt');
      fs.makeDirectoryRecursive(this.rootDir);
      fs.write(this.restKeyFile, "Open Sesame!Open Sesame!Open Ses");
    }
    this.httpAuthOptions = pu.makeAuthorizationHeaders(this.options);
  }
  
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief starts any sort server instance
  // /
  // / protocol must be one of ["tcp", "ssl", "unix"]
  // //////////////////////////////////////////////////////////////////////////////

  prepareInstance () {
    const startTime = time();
    try {
      if (this.options.hasOwnProperty('server')) {
        /// external server, not managed by us.
        this.endpoint = this.options.server,
        this.rootDir = this.options.serverRoot,
        this.url = this.options.server.replace('tcp', 'http'),
        arango.reconnect(this.endpoint, '_system', 'root', '');
        return rc;
      }

      for (let count = 0;
           this.options.agency && count < this.agencyConfig.agencySize;
           count ++) {
        this.arangods.push(new oneInstance(this.options,
                                           instanceRole.agent,
                                           this.addArgs,
                                           this.httpAuthOptions,
                                           this.protocol,
                                           fs.join(this.rootDir, instanceRole.agent + "_" + count),
                                           this.restKeyFile,
                                           this.agencyConfig));
      }
      
      for (let count = 0;
           this.options.cluster && count < this.options.dbServers;
           count ++) {
        this.arangods.push(new oneInstance(this.options,
                                           instanceRole.dbServer,
                                           this.addArgs,
                                           this.httpAuthOptions,
                                           this.protocol,
                                           fs.join(this.rootDir, instanceRole.dbServer + "_" + count),
                                           this.restKeyFile,
                                           this.agencyConfig));
      }
      
      for (let count = 0;
           this.options.cluster && count < this.options.coordinators;
           count ++) {
        this.arangods.push(new oneInstance(this.options,
                                           instanceRole.coordinator,
                                           this.addArgs,
                                           this.httpAuthOptions,
                                           this.protocol,
                                           fs.join(this.rootDir, instanceRole.coordinator + "_" + count),
                                           this.restKeyFile,
                                           this.agencyConfig));
      }
      
      for (let count = 0;
           this.options.activefailover && count < this.singles;
           count ++) {
        this.arangods.push(new oneInstance(this.options,
                                           instanceRole.singles,
                                           this.addArgs,
                                           this.httpAuthOptions,
                                           this.protocol,
                                           fs.join(this.rootDir, instanceRole.failover + "_" + count),
                                           this.restKeyFile,
                                           this.agencyConfig));
      }

      if (!this.options.agency) {
        // Single server...
        this.arangods.push(new oneInstance(this.options,
                                           instanceRole.single,
                                           this.addArgs,
                                           this.httpAuthOptions,
                                           this.protocol,
                                           fs.join(this.rootDir, instanceRole.single + "_" + count),
                                           this.restKeyFile,
                                           this.agencyConfig));
      }

      this.arangods.forEach(arangod => arangod.startArango());
      launchFinalize(options, instanceInfo, startTime);
    } catch (e) {
      print(e, e.stack);
      return false;
    }
    return instanceInfo;
  }

  restartOneInstance(moreArgs) {
    /// todo
    if (this.options.cluster && !this.options.skipReconnect) {
      checkClusterAlive(this.options, instanceInfo, {}); // todo addArgs
      print("reconnecting " + instanceInfo.endpoint);
      arango.reconnect(instanceInfo.endpoint,
                       '_system',
                       this.options.username,
                       this.options.password,
                       false
                      );
    }
    launchFinalize(this.options, instanceInfo, startTime);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief scans the log files for important infos
  // //////////////////////////////////////////////////////////////////////////////

  readImportantLogLines (logPath) {
    let importantLines = {};
    // TODO: iterate over instances
    // for (let i = 0; i < list.length; i++) {
    // }

    return importantLines;
  }

  killWithCoreDump (options, instanceInfo) {
    if (platform.substr(0, 3) === 'win') {
      if (!options.disableMonitor) {
        crashUtils.stopProcdump (options, instanceInfo, true);
      }
      crashUtils.runProcdump (options, instanceInfo, instanceInfo.rootDir, instanceInfo.pid, true);
    }
    instanceInfo.exitStatus = killExternal(instanceInfo.pid, abortSignal);
  }


  initProcessStats(instanceInfo) {
    instanceInfo.arangods.forEach((arangod) => {
      arangod.stats = getProcessStats(arangod.pid);
    });
  }

  getDeltaProcessStats(instanceInfo) {
    try {
      let deltaStats = {};
      let deltaSum = {};
      this.arangods.forEach((arangod) => {
        let newStats = arangod.getProcessStats();
        let myDeltaStats = {};
        for (let key in arangod.stats) {
          if (key.startsWith('sockstat_')) {
            myDeltaStats[key] = newStats[key];
          } else {
            myDeltaStats[key] = newStats[key] - arangod.stats[key];
          }
        }
        deltaStats[arangod.pid + '_' + arangod.role] = myDeltaStats;
        arangod.stats = newStats;
        for (let key in myDeltaStats) {
          if (deltaSum.hasOwnProperty(key)) {
            deltaSum[key] += myDeltaStats[key];
          } else {
            deltaSum[key] = myDeltaStats[key];
          }
        }
      });
      deltaStats['sum_servers'] = deltaSum;
      return deltaStats;
    }
    catch (x) {
      print("aborting stats generation");
      return {};
    }
  }

  summarizeStats(deltaStats) {
    let sumStats = {};
    for (let instance in deltaStats) {
      for (let key in deltaStats[instance]) {
        if (!sumStats.hasOwnProperty(key)) {
          sumStats[key] = 0;
        }
        sumStats[key] += deltaStats[instance][key];
      }
    }
    return sumStats;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief aggregates information from /proc about the SUT
  // //////////////////////////////////////////////////////////////////////////////

  getMemProfSnapshot(instanceInfo, options, counter) {
    if (options.memprof) {
      let opts = Object.assign(pu.makeAuthorizationHeaders(this.options),
                               { method: 'GET' });

      instanceInfo.arangods.forEach((arangod) => {
        let fn = fs.join(arangod.rootDir, `${arangod.role}_${arangod.pid}_${counter}_.heap`);
        let heapdumpReply = download(arangod.url + '/_admin/status?memory=true', opts);
        if (heapdumpReply.code === 200) {
          fs.write(fn, heapdumpReply.body);
          print(CYAN + Date() + ` Saved ${fn}` + RESET);
        } else {
          print(RED + Date() + ` Acquiring Heapdump for ${fn} failed!` + RESET);
          print(heapdumpReply);
        }

        let fnMetrics = fs.join(arangod.rootDir, `${arangod.role}_${arangod.pid}_${counter}_.metrics`);
        let metricsReply = download(arangod.url + '/_admin/metrics/v2', opts);
        if (metricsReply.code === 200) {
          fs.write(fnMetrics, metricsReply.body);
          print(CYAN + Date() + ` Saved ${fnMetrics}` + RESET);
        } else if (metricsReply.code === 503) {
          print(RED + Date() + ` Acquiring metrics for ${fnMetrics} not possible!` + RESET);
        } else {
          print(RED + Date() + ` Acquiring metrics for ${fnMetrics} failed!` + RESET);
          print(metricsReply);
        }
      });
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // //////////////////////////////////////////////////////////////////////////////
  // / Server up/down utilities
  // //////////////////////////////////////////////////////////////////////////////
  // //////////////////////////////////////////////////////////////////////////////

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief dump the state of the agency to disk. if we still can get one.
  // //////////////////////////////////////////////////////////////////////////////
  dumpAgency(instanceInfo, options) {
    this.arangods.forEach((arangod) => {
      if (arangod.role === "agent") {
        if (arangod.hasOwnProperty('exitStatus')) {
          print(Date() + " this agent is already dead: " + JSON.stringify(arangod));
        } else {
          print(Date() + " Attempting to dump Agent: " + JSON.stringify(arangod));
          arangod.dumpAgent( '/_api/agency/config', 'GET', 'agencyConfig');

          arangod.dumpAgent('/_api/agency/state', 'GET', 'agencyState');

          arangod.dumpAgent('/_api/agency/read', 'POST', 'agencyPlan');
        }
      }
    });
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief the bad has happened, tell it the user and try to gather more
  // /        information about the incident. (arangod wrapper for the crash-utils)
  // //////////////////////////////////////////////////////////////////////////////
  analyzeServerCrash (arangod, checkStr) {
    return crashUtils.analyzeCrash(pu.ARANGOD_BIN, arangod, this.options, checkStr);
  }

  checkUptime () {
    let ret = {};
    let opts = Object.assign(pu.makeAuthorizationHeaders(this.options),
                             { method: 'GET' });
    this.arangods.forEach(arangod => {
      let reply = download(arangod.url + '/_admin/statistics', '', opts);
      if (reply.hasOwnProperty('error') || reply.code !== 200) {
        throw new Error("unable to get statistics reply: " + JSON.stringify(reply));
      }

      let statisticsReply = JSON.parse(reply.body);

      ret [ arangod.name ] = statisticsReply.server.uptime;
    });
    return ret;
  }

  abortSurvivors(arangod, options) {
    print(Date() + " Killing in the name of: ");
    print(arangod);
    arangod.terminateInstance();
  }

  _checkServersGOOD() {
    try {
      const health = this.clusterHealth();
      return instanceInfo.arangods.every((arangod) => {
        if (arangod.role === "agent" || arangod.role === "single") {
          return true;
        }
        if (health.hasOwnProperty(arangod.id)) {
          if (health[arangod.id].Status === "GOOD") {
            return true;
          } else {
            print(RED + "ClusterHealthCheck failed " + arangod.id + " has status "
                  + health[arangod.id].Status + " (which is not equal to GOOD)");
            return false;
          }
        } else {
          print(RED + "ClusterHealthCheck failed " + arangod.id
                + " does not have health property");
          return false;
        }
      });
    } catch(e) {
      print("Error checking cluster health " + e);
      return false;
    }
  }



  // skipHealthCheck can be set to true to avoid a call to /_admin/cluster/health
  // on the coordinator. This is necessary if only the agency is running yet.
  checkInstanceAlive(instanceInfo, options, {skipHealthCheck = false} = {}) {
    if (options.activefailover &&
        instanceInfo.hasOwnProperty('authOpts') &&
        (instanceInfo.url !== instanceInfo.agencyUrl)
       ) {
      // only detect a leader after we actually know one has been started.
      // the agency won't tell us anything about leaders.
      let d = detectCurrentLeader(instanceInfo);
      if (instanceInfo.endpoint !== d.endpoint) {
        print(Date() + ' failover has happened, leader is no more! Marking Crashy!');
        serverCrashedLocal = true;
        this.dumpAgency(options);
        return false;
      }
    }

    let rc = this.arangods.reduce((previous, arangod) => {
      let ret = checkArangoAlive(arangod, options);
      if (!ret) {
        instanceInfo.message += arangod.message;
      }
      return previous && ret;
    }, true);
    if (rc && options.cluster && !skipHealthCheck && instanceInfo.arangods.length > 1) {
      const seconds = x => x * 1000;
      const checkAllAlive = () => instanceInfo.arangods.every(arangod => checkArangoAlive(arangod, options));
      let first = true;
      rc = false;
      for (
        const start = Date.now();
        !rc && Date.now() < start + seconds(60) && checkAllAlive();
        internal.sleep(1)
      ) {
        rc = this._checkServersGOOD();
        if (first) {
          if (!options.noStartStopLogs) {
            print(RESET + "Waiting for all servers to go GOOD...");
          }
          first = false;
        }
      }
    }
    if (!rc) {
      this.dumpAgency(options);
      print(Date() + ' If cluster - will now start killing the rest.');
      instanceInfo.arangods.forEach((arangod) => {
        abortSurvivors(arangod, options);
      });
    }
    return rc;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief checks whether any instance has failure points set
  // //////////////////////////////////////////////////////////////////////////////

  checkServerFailurePoints(instanceInfo) {
    let failurePoints = [];
    instanceInfo.arangods.forEach(arangod => {
      // we don't have JWT success atm, so if, skip:
      if ((arangod.role !== "agent") &&
          !arangod.args.hasOwnProperty('server.jwt-secret-folder') &&
          !arangod.args.hasOwnProperty('server.jwt-secret')) {
        let fp = debugGetFailurePoints(arangod.endpoint);
        if (fp.length > 0) {
          failurePoints.push({
            "role": arangod.role,
            "pid":  arangod.pid,
            "database.directory": arangod['database.directory'],
            "failurePoints": fp
          });
        }
      }
    });
    return failurePoints;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief waits for garbage collection using /_admin/execute
  // //////////////////////////////////////////////////////////////////////////////

  waitOnServerForGC (instanceInfo, options, waitTime) {
    try {
      print(Date() + ' waiting ' + waitTime + ' for server GC');
      const remoteCommand = 'require("internal").wait(' + waitTime + ', true);';

      const requestOptions = pu.makeAuthorizationHeaders(this.options);
      requestOptions.method = 'POST';
      requestOptions.timeout = waitTime * 10;
      requestOptions.returnBodyOnError = true;

      const reply = download(
        instanceInfo.url + '/_admin/execute?returnAsJSON=true',
        remoteCommand,
        requestOptions);

      print(Date() + ' waiting ' + waitTime + ' for server GC - done.');

      if (!reply.error && reply.code === 200) {
        return JSON.parse(reply.body);
      } else {
        return {
          status: false,
          message: yaml.safedump(reply.body)
        };
      }
    } catch (ex) {
      return {
        status: false,
        message: ex.message || String(ex),
        stack: ex.stack
      };
    }
  }


  // //////////////////////////////////////////////////////////////////////////////
  // / @brief on linux get a statistic about the sockets we used
  // //////////////////////////////////////////////////////////////////////////////

  getSockStat(arangod, options, preamble) {
    if (this.options.getSockStat && (platform === 'linux')) {
      let sockStat = preamble + arangod.pid + "\n";
      sockStat += getSockStatFile(arangod.pid);
      return sockStat;
    }
  }
  getSockStatFile(pid) {
    try {
      return fs.read("/proc/" + pid + "/net/sockstat");
    } catch (e) {/* oops, process already gone? don't care. */ }
    return "";
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief commands a server to shut down via webcall
  // //////////////////////////////////////////////////////////////////////////////

  shutdownArangod (arangod, options, forceTerminate) {
    if (forceTerminate === undefined) {
      forceTerminate = false;
    }
    if (options.hasOwnProperty('server')) {
      print(Date() + ' running with external server');
      return;
    }

    if (options.valgrind) {
      waitOnServerForGC(arangod, options, 60);
    }
    if (options.rr && forceTerminate) {
      forceTerminate = false;
      options.useKillExternal = true;
    }
    if ((!arangod.hasOwnProperty('exitStatus')) ||
        (arangod.exitStatus.status === 'RUNNING')) {
      if (forceTerminate) {
        let sockStat = this.getSockStat(arangod, options, "Force killing - sockstat before: ");
        killWithCoreDump(options, arangod);
        analyzeServerCrash(arangod, options, 'shutdown timeout; instance forcefully KILLED because of fatal timeout in testrun ' + sockStat);
      } else if (options.useKillExternal) {
        let sockStat = this.getSockStat(arangod, options, "Shutdown by kill - sockstat before: ");
        arangod.exitStatus = killExternal(arangod.pid);
        print(sockStat);
      } else {
        const requestOptions = pu.makeAuthorizationHeaders(this.options);
        requestOptions.method = 'DELETE';
        requestOptions.timeout = 60; // 60 seconds hopefully are enough for getting a response
        if (!options.noStartStopLogs) {
          print(Date() + ' ' + arangod.url + '/_admin/shutdown');
        }
        let sockStat = this.getSockStat(arangod, options, "Sock stat for: ");
        const reply = download(arangod.url + '/_admin/shutdown', '', requestOptions);
        if ((reply.code !== 200) && // if the server should reply, we expect 200 - if not:
            !((reply.code === 500) &&
              (
                (reply.message === "Connection closed by remote") || // http connection
                  reply.message.includes('failed with #111')           // https connection
              ))) {
          serverCrashedLocal = true;
          print(Date() + ' Wrong shutdown response: ' + JSON.stringify(reply) + "' " + sockStat + " continuing with hard kill!");
          shutdownArangod(arangod, options, true);
        }
        else if (!options.noStartStopLogs) {
          print(sockStat);
        }
        if (options.extremeVerbosity) {
          print(Date() + ' Shutdown response: ' + JSON.stringify(reply));
        }
      }
    } else {
      print(Date() + ' Server already dead, doing nothing.');
    }
  }


  shutDownOneInstance(options, arangod, fullInstance, counters, forceTerminate, timeout) {
    let shutdownTime = internal.time();
    if (!arangod.hasOwnProperty('exitStatus')) {
      shutdownArangod(arangod, options, forceTerminate);
      if (forceTerminate) {
        print(Date() + " FORCED shut down: " + JSON.stringify(arangod));
      } else {
        arangod.exitStatus = {
          status: 'RUNNING'
        };
        print(Date() + " Commanded shut down: " + JSON.stringify(arangod));
      }
      return true;
    }
    if (arangod.exitStatus.status === 'RUNNING') {
      arangod.exitStatus = statusExternal(arangod.pid, false);
      if (!crashUtils.checkMonitorAlive(pu.ARANGOD_BIN, arangod, options, arangod.exitStatus)) {
        if (arangod.role !== 'agent') {
          counters.nonAgenciesCount--;
        }
        print(Date() + ' Server "' + arangod.role + '" shutdown: detected irregular death by monitor: pid', arangod.pid);
        return false;
      }
    }
    if (arangod.exitStatus.status === 'RUNNING') {
      let localTimeout = timeout;
      if (arangod.role === 'agent') {
        localTimeout = localTimeout + 60;
      }
      if ((internal.time() - shutdownTime) > localTimeout) {
        this.dumpAgency(fullInstance, options);
        print(Date() + ' forcefully terminating ' + yaml.safeDump(arangod) +
              ' after ' + timeout + 's grace period; marking crashy.');
        serverCrashedLocal = true;
        counters.shutdownSuccess = false;
        killWithCoreDump(options, arangod);
        analyzeServerCrash(arangod,
                           options,
                           'shutdown timeout; instance "' +
                           arangod.role +
                           '" forcefully KILLED after 60s - ' +
                           arangod.exitStatus.signal);
        if (arangod.role !== 'agent') {
          counters.nonAgenciesCount--;
        }
        return false;
      } else {
        return true;
      }
    } else if (arangod.exitStatus.status !== 'TERMINATED') {
      if (arangod.role !== 'agent') {
        counters.nonAgenciesCount--;
      }
      if (arangod.exitStatus.hasOwnProperty('signal') || arangod.exitStatus.hasOwnProperty('monitor')) {
        analyzeServerCrash(arangod, options, 'instance "' + arangod.role + '" Shutdown - ' + arangod.exitStatus.signal);
        print(Date() + " shutdownInstance: Marking crashy - " + JSON.stringify(arangod));
        serverCrashedLocal = true;
        counters.shutdownSuccess = false;
      }
      crashUtils.stopProcdump(options, arangod);
    } else {
      if (arangod.role !== 'agent') {
        counters.nonAgenciesCount--;
      }
      if (!options.noStartStopLogs) {
        print(Date() + ' Server "' + arangod.role + '" shutdown: Success: pid', arangod.pid);
      }
      crashUtils.stopProcdump(options, arangod);
      return false;
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief shuts down an instance
  // //////////////////////////////////////////////////////////////////////////////

  shutdownInstance (instanceInfo, options, forceTerminate) {
    if (forceTerminate === undefined) {
      forceTerminate = false;
    }
    let shutdownSuccess = !forceTerminate;

    // we need to find the leading server
    if (options.activefailover) {
      let d = detectCurrentLeader(instanceInfo);
      if (instanceInfo.endpoint !== d.endpoint) {
        print(Date() + ' failover has happened, leader is no more! Marking Crashy!');
        serverCrashedLocal = true;
        forceTerminate = true;
        shutdownSuccess = false;
      }
    }

    if (!checkInstanceAlive(instanceInfo, options)) {
      print(Date() + ' Server already dead, doing nothing. This shouldn\'t happen?');
    }

    if (!forceTerminate) {
      try {
        // send a maintenance request to any of the coordinators, so that
        // no failed server/failed follower jobs will be started on shutdown
        let coords = instanceInfo.arangods.filter(arangod =>
          arangod.role === 'coordinator' &&
            !arangod.hasOwnProperty('exitStatus'));
        if (coords.length > 0) {
          let requestOptions = pu.makeAuthorizationHeaders(this.options);
          requestOptions.method = 'PUT';

          if (!options.noStartStopLogs) {
            print(coords[0].url + "/_admin/cluster/maintenance");
          }
          download(coords[0].url + "/_admin/cluster/maintenance", JSON.stringify("on"), requestOptions);
        }
      } catch (err) {
        print(Date() + " error while setting cluster maintenance mode:", err);
        shutdownSuccess = false;
      }
    }

    if (options.cluster && instanceInfo.hasOwnProperty('clusterHealthMonitor')) {
      try {
        instanceInfo.clusterHealthMonitor['kill'] = killExternal(instanceInfo.clusterHealthMonitor.pid);
        instanceInfo.clusterHealthMonitor['statusExternal'] = statusExternal(instanceInfo.clusterHealthMonitor.pid, true);
      }
      catch (x) {
        print(x);
      }
    }

    // Shut down all non-agency servers:
    const n = instanceInfo.arangods.length;

    let toShutdown = instanceInfo.arangods.slice();
    toShutdown.sort((a, b) => {
      if (a.role === b.role) return 0;
      if (a.role === 'coordinator' &&
          b.role === 'dbserver') return -1;
      if (b.role === 'coordinator' &&
          a.role === 'dbserver') return 1;
      if (a.role === 'agent') return 1;
      if (b.role === 'agent') return -1;
      return 0;
    });

    if (!options.noStartStopLogs) {
      print(Date() + ' Shutdown order ' + JSON.stringify(toShutdown));
    }

    let nonAgenciesCount = instanceInfo.arangods
        .filter(arangod => {
          if (arangod.hasOwnProperty('exitStatus') &&
              (arangod.exitStatus.status !== 'RUNNING')) {
            return false;
          }
          return (arangod.role !== 'agent');
        }).length;

    let timeout = 666;
    if (options.valgrind) {
      timeout *= 10;
    }
    if (options.sanitizer) {
      timeout *= 2;
    }

    if ((toShutdown.length > 0) && (options.agency === true) && (options.dumpAgencyOnError === true)) {
      this.dumpAgency(instanceInfo, options);
    }
    var shutdownTime = internal.time();
    while (toShutdown.length > 0) {
      toShutdown = toShutdown.filter(arangod => {
        if (!arangod.hasOwnProperty('exitStatus')) {
          if ((nonAgenciesCount > 0) && (arangod.role === 'agent')) {
            return true;
          }
          shutdownArangod(arangod, options, forceTerminate);
          if (forceTerminate) {
            print(Date() + " FORCED shut down: " + JSON.stringify(arangod));
          } else {
            arangod.exitStatus = {
              status: 'RUNNING'
            };

            if (!options.noStartStopLogs) {
              print(Date() + " Commanded shut down: " + JSON.stringify(arangod));
            }
          }
          return true;
        }
        if (arangod.exitStatus.status === 'RUNNING') {
          arangod.exitStatus = statusExternal(arangod.pid, false);
          if (!crashUtils.checkMonitorAlive(pu.ARANGOD_BIN, arangod, options, arangod.exitStatus)) {
            if (arangod.role !== 'agent') {
              nonAgenciesCount--;
            }
            print(Date() + ' Server "' + arangod.role + '" shutdown: detected irregular death by monitor: pid', arangod.pid);
            return false;
          }
        }
        if (arangod.exitStatus.status === 'RUNNING') {
          let localTimeout = timeout;
          if (arangod.role === 'agent') {
            localTimeout = localTimeout + 60;
          }
          if ((internal.time() - shutdownTime) > localTimeout) {
            this.dumpAgency(instanceInfo, options);
            print(Date() + ' forcefully terminating ' + yaml.safeDump(arangod) +
                  ' after ' + timeout + 's grace period; marking crashy.');
            serverCrashedLocal = true;
            shutdownSuccess = false;
            killWithCoreDump(options, arangod);
            analyzeServerCrash(arangod,
                               options,
                               'shutdown timeout; instance "' +
                               arangod.role +
                               '" forcefully KILLED after 60s - ' +
                               arangod.exitStatus.signal);
            if (arangod.role !== 'agent') {
              nonAgenciesCount--;
            }
            return false;
          } else {
            return true;
          }
        } else if (arangod.exitStatus.status !== 'TERMINATED') {
          if (arangod.role !== 'agent') {
            nonAgenciesCount--;
          }
          if (arangod.exitStatus.hasOwnProperty('signal') || arangod.exitStatus.hasOwnProperty('monitor')) {
            analyzeServerCrash(arangod, options, 'instance "' + arangod.role + '" Shutdown - ' + arangod.exitStatus.signal);
            print(Date() + " shutdownInstance: Marking crashy - " + JSON.stringify(arangod));
            serverCrashedLocal = true;
            shutdownSuccess = false;
          }
          crashUtils.stopProcdump(options, arangod);
        } else {
          if (arangod.role !== 'agent') {
            nonAgenciesCount--;
          }
          if (!options.noStartStopLogs) {
            print(Date() + ' Server "' + arangod.role + '" shutdown: Success: pid', arangod.pid);
          }
          crashUtils.stopProcdump(options, arangod);
          return false;
        }
      });
      if (toShutdown.length > 0) {
        let roles = {};
        toShutdown.forEach(arangod => {
          if (!roles.hasOwnProperty(arangod.role)) {
            roles[arangod.role] = 0;
          }
          ++roles[arangod.role];
        });
        let roleNames = [];
        for (let r in roles) {
          // e.g. 2 + coordinator + (s)
          roleNames.push(roles[r] + ' ' + r + '(s)');
        }
        if (!options.noStartStopLogs) {
          print(roleNames.join(', ') + ' are still running...');
        }
        require('internal').wait(1, false);
      }
    }

    if (!options.skipLogAnalysis) {
      instanceInfo.arangods.forEach(arangod => {
        let errorEntries = readImportantLogLines(arangod.rootDir);
        if (Object.keys(errorEntries).length > 0) {
          print(Date() + ' Found messages in the server logs: \n' +
                yaml.safeDump(errorEntries));
        }
      });
    }
    instanceInfo.arangods.forEach(arangod => {
      readAssertLogLines(arangod.rootDir);
    });
    if (tcpdump !== undefined) {
      print(CYAN + "Stopping tcpdump" + RESET);
      killExternal(tcpdump.pid);
      try {
        statusExternal(tcpdump.pid, true);
      } catch (x)
      {
        print(Date() + ' wasn\'t able to stop tcpdump: ' + x.message );
      }
    }
    cleanupDirectories.unshift(instanceInfo.rootDir);
    return shutdownSuccess;
  }

  detectCurrentLeader(instanceInfo) {
    let opts = {
      method: 'POST',
      jwt: crypto.jwtEncode(instanceInfo.authOpts['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256'),
      headers: {'content-type': 'application/json' }
    };
    let reply = download(instanceInfo.agencyUrl + '/_api/agency/read', '[["/arango/Plan/AsyncReplication/Leader"]]', opts);

    if (!reply.error && reply.code === 200) {
      let res = JSON.parse(reply.body);
      //internal.print("Response ====> " + reply.body);
      let leader = res[0].arango.Plan.AsyncReplication.Leader;
      if (!leader) {
        throw "Leader is not selected";
      }
    }
    opts['method'] = 'GET';
    reply = download(instanceInfo.url + '/_api/cluster/endpoints', '', opts);
    let res;
    try {
      res = JSON.parse(reply.body);
    }
    catch (x) {
      throw "Failed to parse endpoints reply: " + JSON.stringify(reply);
    }
    let leader = res.endpoints[0].endpoint;
    let leaderInstance;
    instanceInfo.arangods.forEach(d => {
      if (d.endpoint === leader) {
        leaderInstance = d;
      }
    });
    return leaderInstance;
  }

  checkClusterAlive(options, instanceInfo, addArgs) {
    let httpOptions = _.clone(this.httpAuthOptions)
    httpOptions.method = 'POST';
    httpOptions.returnBodyOnError = true;

    // scrape the jwt token
    instanceInfo.authOpts = _.clone(options);
    if (addArgs['server.jwt-secret'] && !instanceInfo.authOpts['server.jwt-secret']) {
      instanceInfo.authOpts['server.jwt-secret'] = addArgs['server.jwt-secret'];
    } else if (addArgs['server.jwt-secret-folder'] && !instanceInfo.authOpts['server.jwt-secret-folder']) {
      instanceInfo.authOpts['server.jwt-secret-folder'] = addArgs['server.jwt-secret-folder'];
    }


    let count = 0;
    while (true) {
      ++count;

      instanceInfo.arangods.forEach(arangod => {
        if (arangod.suspended || arangod.upAndRunning) {
          // just fake the availability here
          arangod.upAndRunning = true;
          return;
        }
        if (!options.noStartStopLogs) {
          print(Date() + " tickeling cluster node " + arangod.url + " - " + arangod.role);
        }
        let url = arangod.url;
        if (arangod.role === "coordinator" && arangod.args["javascript.enabled"] !== "false") {
          url += '/_admin/aardvark/index.html';
        } else {
          url += '/_api/version';
        }
        const reply = download(url, '', pu.makeAuthorizationHeaders(instanceInfo.authOpts));
        if (!reply.error && reply.code === 200) {
          arangod.upAndRunning = true;
          return true;
        }

        if (!checkArangoAlive(arangod, options)) {
          instanceInfo.arangods.forEach(arangod => {
            if (!arangod.hasOwnProperty('exitStatus') ||
                (arangod.exitStatus.status === 'RUNNING')) {
              // killWithCoreDump(options, arangod);
              arangod.exitStatus = killExternal(arangod.pid, termSignal);
            }
            analyzeServerCrash(arangod, options, 'startup timeout; forcefully terminating ' + arangod.role + ' with pid: ' + arangod.pid);
          });

          throw new Error(`cluster startup: pid ${arangod.pid} no longer alive! bailing out!`);
        }
        wait(0.5, false);
        return true;
      });

      let upAndRunning = 0;
      instanceInfo.arangods.forEach(arangod => {
        if (arangod.upAndRunning) {
          upAndRunning += 1;
        }
      });
      if (upAndRunning === instanceInfo.arangods.length) {
        break;
      }

      // Didn't startup in 10 minutes? kill it, give up.
      if (count > 1200) {
        instanceInfo.arangods.forEach(arangod => {
          killWithCoreDump(options, arangod);
          analyzeServerCrash(arangod, options, 'startup timeout; forcefully terminating ' + arangod.role + ' with pid: ' + arangod.pid);
        });
        throw new Error('cluster startup timed out after 10 minutes!');
      }
    }

    if (!options.noStartStopLogs) {
      print("Determining server IDs");
    }
    instanceInfo.arangods.forEach(arangod => {
      if (arangod.suspended) {
        return;
      }
      // agents don't support the ID call...
      if ((arangod.role !== "agent") && (arangod.role !== "single")) {
        let reply;
        try {
          reply = download(arangod.url + '/_db/_system/_admin/server/id', '', pu.makeAuthorizationHeaders(instanceInfo.authOpts));
        } catch (e) {
          print(RED + Date() + " error requesting server '" + JSON.stringify(arangod) + "' Error: " + JSON.stringify(e) + RESET);
          if (e instanceof ArangoError && e.message.search('Connection reset by peer') >= 0) {
            internal.sleep(5);
            reply = download(arangod.url + '/_db/_system/_admin/server/id', '', pu.makeAuthorizationHeaders(instanceInfo.authOpts));
          } else {
            throw e;
          }
        }
        if (reply.error || reply.code !== 200) {
          throw new Error("Server has no detectable ID! " + JSON.stringify(reply) + "\n" + JSON.stringify(arangod));
        }
        let res = JSON.parse(reply.body);
        arangod.id = res['id'];
      }
    });

    if ((options.cluster || options.agency) &&
        !instanceInfo.hasOwnProperty('clusterHealthMonitor') &&
        !options.disableClusterMonitor) {
      print("spawning cluster health inspector");
      internal.env.INSTANCEINFO = JSON.stringify(instanceInfo);
      internal.env.OPTIONS = JSON.stringify(options);
      let args = makeArgsArangosh(options);
      args['javascript.execute'] = fs.join('js', 'client', 'modules', '@arangodb', 'testutils', 'clusterstats.js');
      const argv = toArgv(args);
      instanceInfo.clusterHealthMonitor = executeExternal(pu.ARANGOSH_BIN, argv);
      instanceInfo.clusterHealthMonitorFile = fs.join(instanceInfo.rootDir, 'stats.jsonl');
    }
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief starts an instance
  // /
  // / protocol must be one of ["tcp", "ssl", "unix"]
  // //////////////////////////////////////////////////////////////////////////////

  startInstanceCluster (instanceInfo, protocol, options,
                                 addArgs, rootDir) {
    if (options.cluster && options.activefailover ||
        !options.cluster && !options.activefailover) {
      throw "invalid call to startInstanceCluster";
    }

    startInstanceAgency(instanceInfo, protocol, options, ...makeArgs('agency', 'agency', {}));

    let agencyEndpoint = instanceInfo.endpoint;
    instanceInfo.agencyUrl = instanceInfo.url;
    if (!checkInstanceAlive(instanceInfo, options, {skipHealthCheck: true})) {
      throw new Error('startup of agency failed! bailing out!');
    }

    let i;
    if (options.cluster) {
      for (i = 0; i < options.dbServers; i++) {
        startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('dbserver' + i, 'dbserver', primaryArgs), 'dbserver');
      }

      for (i = 0; i < options.coordinators; i++) {

        startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('coordinator' + i, 'coordinator', coordinatorArgs), 'coordinator');
      }
    } else if (options.activefailover) {
      for (i = 0; i < options.singles; i++) {
        startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('single' + i, 'single', singleArgs), 'single');
        sleep(1.0);
      }
    }

    
    checkClusterAlive(options, instanceInfo, addArgs);

    // we need to find the leading server
    if (options.activefailover) {
      internal.wait(5.0, false);
      let d = detectCurrentLeader(instanceInfo);
      if (d === undefined) {
        throw "failed to detect a leader";
      }
      instanceInfo.endpoint = d.endpoint;
      instanceInfo.url = d.url;
    }

    try {
      arango.reconnect(instanceInfo.endpoint, '_system', 'root', '');
    } catch (e) {
      print(RED + Date() + " error connecting '" + instanceInfo.endpoint + "' Error: " + JSON.stringify(e) + RESET);
      if (e instanceof ArangoError && e.message.search('Connection reset by peer') >= 0) {
        internal.sleep(5);
        arango.reconnect(instanceInfo.endpoint, '_system', 'root', '');
      } else {
        throw e;
      }
    }
    return true;
  }

  launchFinalize(options, instanceInfo, startTime) {
    if (!options.cluster) {
      let count = 0;
      instanceInfo.arangods.forEach(arangod => {
        if (arangod.suspended) {
          return;
        }
        while (true) {
          wait(1, false);
          if (options.useReconnect) {
            try {
              print(Date() + " reconnecting " + arangod.url);
              arango.reconnect(instanceInfo.endpoint,
                               '_system',
                               options.username,
                               options.password,
                               count > 50
                              );
              break;
            } catch (e) {
              instanceInfo.arangods.forEach( arangod => {
                let status = statusExternal(arangod.pid, false);
                if (status.status !== "RUNNING") {
                  throw new Error(`Arangod ${arangod.pid} exited instantly! ` + JSON.stringify(status));
                }
              });
              print(Date() + " caught exception: " + e.message);
            }
          } else {
            print(Date() + " tickeling " + arangod.url);
            const reply = download(arangod.url + '/_api/version', '', pu.makeAuthorizationHeaders(this.options));

            if (!reply.error && reply.code === 200) {
              break;
            }
          }
          ++count;

          if (count % 60 === 0) {
            if (!checkArangoAlive(arangod, options)) {
              throw new Error('startup failed! bailing out!');
            }
          }
          if (count === 300) {
            throw new Error('startup timed out! bailing out!');
          }
        }
      });
      instanceInfo.endpoints = [instanceInfo.endpoint];
      instanceInfo.urls = [instanceInfo.url];
    } else {
      instanceInfo.urls = [];
      instanceInfo.endpoints = [];
      instanceInfo.arangods.forEach(arangod => {
        if (arangod.role === 'coordinator') {
          instanceInfo.urls.push(arangod.url);
          instanceInfo.endpoints.push(arangod.endpoint);
        }
      });
    }
    if (!options.noStartStopLogs) {
      print(CYAN + Date() + ' up and running in ' + (time() - startTime) + ' seconds' + RESET);
    }
    var matchPort = /.*:.*:([0-9]*)/;
    var ports = [];
    var processInfo = [];
    instanceInfo.arangods.forEach(arangod => {
      let res = matchPort.exec(arangod.endpoint);
      if (!res) {
        return;
      }
      var port = res[1];
      if (arangod.role === 'agent') {
        if (options.sniffAgency) {
          ports.push('port ' + port);
        }
      } else if (arangod.role === 'dbserver') {
        if (options.sniffDBServers) {
          ports.push('port ' + port);
        }
      } else {
        ports.push('port ' + port);
      }
      processInfo.push('  [' + arangod.role +
                       '] up with pid ' + arangod.pid +
                       ' on port ' + port +
                       ' - ' + arangod.args['database.directory']);
    });

    if (options.sniff !== undefined && options.sniff !== false) {
      options.cleanup = false;
      let device = 'lo';
      if (platform.substr(0, 3) === 'win') {
        device = '1';
      }
      if (options.sniffDevice !== undefined) {
        device = options.sniffDevice;
      }

      let prog = 'tcpdump';
      if (platform.substr(0, 3) === 'win') {
        prog = 'c:/Program Files/Wireshark/tshark.exe';
      }
      if (options.sniffProgram !== undefined) {
        prog = options.sniffProgram;
      }

      let pcapFile = fs.join(instanceInfo.rootDir, 'out.pcap');
      let args;
      if (prog === 'ngrep') {
        args = ['-l', '-Wbyline', '-d', device];
      } else {
        args = ['-ni', device, '-s0', '-w', pcapFile];
      }
      for (let port = 0; port < ports.length; port ++) {
        if (port > 0) {
          args.push('or');
        }
        args.push(ports[port]);
      }

      if (options.sniff === 'sudo') {
        args.unshift(prog);
        prog = 'sudo';
      }
      print(CYAN + 'launching ' + prog + ' ' + JSON.stringify(args) + RESET);
      tcpdump = executeExternal(prog, args);
    }
    if (!options.noStartStopLogs) {
      print(processInfo.join('\n') + '\n');
    }
    internal.sleep(options.sleepBeforeStart);
    if (!options.disableClusterMonitor) {
      initProcessStats(instanceInfo);
    }
  }


  // //////////////////////////////////////////////////////////////////////////////
  // / @brief starts an agency instance
  // /
  // / protocol must be one of ["tcp", "ssl", "unix"]
  // //////////////////////////////////////////////////////////////////////////////

  startInstanceAgency (instanceInfo, protocol, options, addArgs, rootDir) {

    checkClusterAlive(options, instanceInfo, addArgs);

    return instanceInfo;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief starts a single server instance
  // /
  // / protocol must be one of ["tcp", "ssl", "unix"]
  // //////////////////////////////////////////////////////////////////////////////

  startInstanceSingleServer (instanceInfo, protocol, options,
                                      addArgs, rootDir, role) {
    instanceInfo.arangods.push(startArango(protocol, options, addArgs, rootDir, role));

    instanceInfo.endpoint = instanceInfo.arangods[instanceInfo.arangods.length - 1].endpoint;
    instanceInfo.url = instanceInfo.arangods[instanceInfo.arangods.length - 1].url;
    // instanceInfo.vstEndpoint = instanceInfo.arangods[instanceInfo.arangods.length - 1].url.replace(/.*\/\//, 'vst://');

    return instanceInfo;
  }



  reStartInstance(options, instanceInfo, moreArgs) {

    const startTime = time();

    instanceInfo.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      delete(oneInstance.exitStatus);
      delete(oneInstance.pid);
      oneInstance.upAndRunning = false;
    });

    if (options.cluster) {
      let agencyInstance = {arangods: []};
      instanceInfo.arangods.forEach(function (oneInstance, i) {
        if (oneInstance.pid) {
          return;
        }
        if (oneInstance.role === 'agent') {
          print("relaunching: " + JSON.stringify(oneInstance));
          launchInstance(options, oneInstance, moreArgs);
          agencyInstance.arangods.push(_.clone(oneInstance));
        }
      });
      if (!checkInstanceAlive(agencyInstance, options, {skipHealthCheck: true})) {
        throw new Error('startup of agency failed! bailing out!');
      }
    }

    instanceInfo.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      if ((oneInstance.role === 'PRIMARY') ||
          (oneInstance.role === 'primary') ||
          (oneInstance.role === 'dbserver')) {
        print("relaunching: " + JSON.stringify(oneInstance));
        launchInstance(options, oneInstance, moreArgs);
      }
    });
    instanceInfo.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      if ((oneInstance.role === 'COORDINATOR') || (oneInstance.role === 'coordinator')) {
        print("relaunching: " + JSON.stringify(oneInstance));
        launchInstance(options, oneInstance, moreArgs);
      }
    });
    instanceInfo.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      if (oneInstance.role === 'single') {
        launchInstance(options, oneInstance, moreArgs);
      }
    });

    if (options.cluster && !options.skipReconnect) {
      checkClusterAlive(options, instanceInfo, {}); // todo addArgs
      print("reconnecting " + instanceInfo.endpoint);
      arango.reconnect(instanceInfo.endpoint,
                       '_system',
                       options.username,
                       options.password,
                       false
                      );
    }
    launchFinalize(options, instanceInfo, startTime);
  }
}

exports.instanceManager = instanceManager;
