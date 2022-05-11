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
  constructor(options, wholeCluster) {
    this.wholeCluster = wholeCluster;
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
  getStructure() {
    return {
      agencySize: this.agencySize,
      supervision: this.supervision,
      waitForSync: this.waitForSync,
      agencyEndpoint: this.agencyEndpoint,
      agentsLaunched: this.agentsLaunched,
    };
  }
}

class instance {
  // / protocol must be one of ["tcp", "ssl", "unix"]
  constructor(options, instanceRole, addArgs, authHeaders, protocol, rootDir, restKeyFile, agencyConfig) {
    if (! PORTMANAGER) {
      PORTMANAGER = new portManager(options);
    }
    this.options = options;
    this.instanceRole = instanceRole;
    this.rootDir = rootDir;
    this.protocol = protocol;
    this.args = _.clone(addArgs);
    this.authHeaders = authHeaders;
    this.restKeyFile = restKeyFile;
    this.agencyConfig = agencyConfig;

    this.upAndRunning = false;
    this.suspended = false;
    this.message = '';
    this.port = '';
    this.endpoint = '';

    this.dataDir = fs.join(this.rootDir, 'data');
    this.appDir = fs.join(this.rootDir, 'apps');
    this.tmpDir = fs.join(this.rootDir, 'tmp');

    fs.makeDirectoryRecursive(this.dataDir);
    fs.makeDirectoryRecursive(this.appDir);
    fs.makeDirectoryRecursive(this.tmpDir);
    this.logFile = fs.join(rootDir, 'log');
    this._makeArgsArangod();
    
    this.name = instanceRole + ' - ' + this.port;
    this.pid = null;
    this.exitStatus = null;
  }

  getStructure() {
    return {
      name: this.name,
      instanceRole: this.instanceRole,
      message: this.message,
      rootDir: this.rootDir,
      protocol: this.protocol,
      authHeaders: this.authHeaders,
      restKeyFile: this.restKeyFile,
      agencyConfig: this.agencyConfig.getStructure(),
      upAndRunning: this.upAndRunning,
      suspended: this.suspended,
      port: this.port,
      endpoint: this.endpoint,
      dataDir: this.dataDir,
      appDir: this.appDir,
      tmpDir: this.tmpDir,
      logFile: this.logFile,
      args: this.args,
      pid: this.pid,
      exitStatus: this.exitStatus
    };
  }

  isRole(compareRole) {
    return this.instanceRole === compareRole;
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
    this.url = pu.endpointToURL(this.endpoint);

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
      // arangod.killWithCoreDump();
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

  _executeArangod () {
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
    try {
      this.pid = this._executeArangod().pid;
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
        this.agencyConfig.wholeCluster.checkClusterAlive()
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
      oneInstanceInfo.pid = this._executeArangod(oneInstanceInfo.args).pid;
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
        this.analyzeServerCrash(msg);
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
  killWithCoreDump () {
    if (this.pid === null) {
      return;
    }
    if (platform.substr(0, 3) === 'win') {
      if (!this.options.disableMonitor) {
        crashUtils.stopProcdump (this.options, this, true);
      }
      crashUtils.runProcdump (this.options, this, this.rootDir, this.pid, true);
    }
    this.exitStatus = killExternal(this.pid, abortSignal);
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief the bad has happened, tell it the user and try to gather more
  // /        information about the incident. (arangod wrapper for the crash-utils)
  // //////////////////////////////////////////////////////////////////////////////
  analyzeServerCrash (checkStr) {
    if (this.exitStatus === null) {
      return 'Not yet launched!';
    }
    return crashUtils.analyzeCrash(pu.ARANGOD_BIN, this, this.options, checkStr);
  }

  getMemProfSnapshot(opts) {
    let fn = fs.join(this.rootDir, `${arangod.role}_${arangod.pid}_${counter}_.heap`);
    let heapdumpReply = download(this.url + '/_admin/status?memory=true', opts);
    if (heapdumpReply.code === 200) {
      fs.write(fn, heapdumpReply.body);
      print(CYAN + Date() + ` Saved ${fn}` + RESET);
    } else {
      print(RED + Date() + ` Acquiring Heapdump for ${fn} failed!` + RESET);
      print(heapdumpReply);
    }

    let fnMetrics = fs.join(this.rootDir, `${arangod.role}_${arangod.pid}_${counter}_.metrics`);
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
  }
}


exports.instance = instance;
exports.agencyConfig = agencyConfig;
exports.instanceType = instanceType;
exports.instanceRole = instanceRole;
