/* jshint strict: false, sub: true */
/* global print, arango */
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

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const rp = require('@arangodb/testutils/result-processing');
const pm = require('@arangodb/testutils/portmanager');
const yaml = require('js-yaml');
const internal = require('internal');
const crashUtils = require('@arangodb/testutils/crash-utils');
const {sanHandler} = require('@arangodb/testutils/san-file-handler');
const ArangoError = require('@arangodb').ArangoError;

/* Functions: */
const {
  toArgv,
  executeExternal,
  killExternal,
  statusExternal,
  statisticsExternal,
  suspendExternal,
  continueExternal,
  base64Encode,
  download,
  platform,
  time,
  wait,
  sleep
} = internal;

/* Constants: */
const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = internal.COLORS.COLOR_YELLOW;
const IS_A_TTY = RED.length !== 0;

const abortSignal = 6;
const termSignal = 15;

let tcpdump;

let PORTMANAGER;

const seconds = x => x * 1000;

function getSockStatFile(pid) {
  try {
    return fs.read("/proc/" + pid + "/net/sockstat");
  } catch (e) {/* oops, process already gone? don't care. */ }
  return "";
}


const instanceType = {
  single: 'single',
  cluster: 'cluster'
};

const instanceRole = {
  single: 'single',
  agent: 'agent',
  dbServer: 'dbserver',
  coordinator: 'coordinator',
};

class instance {
  #pid = null;

  // / protocol must be one of ["tcp", "ssl", "unix"]
  constructor(options, instanceRole, addArgs, authHeaders, protocol, rootDir, restKeyFile, agencyMgr, tmpDir, mem) {
    this.id = null;
    this.shortName = null;
    this.pm = pm.getPortManager(options);
    this.options = options;
    this.instanceRole = instanceRole;
    this.rootDir = rootDir;
    this.protocol = protocol;

    this.moreArgs = {};
    this.args = {};
    for (const [key, value] of Object.entries(addArgs)) {
      if (key.search('extraArgs') >= 0) {
        let splitkey = key.split('.');
        if (splitkey.length !== 2) {
          if (splitkey[1] === this.instanceRole) {
            this.args[splitkey.slice(2).join('.')] = value;
          }
        } else {
          this.args[key] = value;
        }
      } else {
        this.args[key] = value;
      }
    }
    this.authHeaders = authHeaders;
    this.restKeyFile = restKeyFile;
    this.agencyMgr = agencyMgr;

    this.upAndRunning = false;
    this.suspended = false;
    this.message = '';
    this.port = '';
    this.url = '';
    this.endpoint = '';
    this.connectionHandle = undefined;
    this.assertLines = [];
    this.useableMemory = mem;
    this.memProfCounter = 0;

    this.topLevelTmpDir = tmpDir;
    this.dataDir = fs.join(this.rootDir, 'data');
    this.appDir = fs.join(this.rootDir, 'apps');
    this.tmpDir = fs.join(this.rootDir, 'tmp');

    fs.makeDirectoryRecursive(this.dataDir);
    fs.makeDirectoryRecursive(this.appDir);
    fs.makeDirectoryRecursive(this.tmpDir);
    this.logFile = fs.join(rootDir, 'log');
    this.coreDirectory = this.rootDir;
    if (process.env.hasOwnProperty('COREDIR')) {
      this.coreDirectory = process.env['COREDIR'];
    }
    this.JWT = null;
    this.jwtFiles = null;
    this.jwtSecrets = [];
    this.sanHandler = new sanHandler('arangod', this.options);

    this._makeArgsArangod();

    this.name = instanceRole + ' - ' + this.port;
    this.exitStatus = null;
    this.serverCrashedLocal = false;
    this.netstat = {'in':{}, 'out': {}};
  }

  set pid(value) {
    if (this.#pid !== null) {
      this.processSanitizerReports();
    }
    this.#pid = value;
  }

  get pid() { return this.#pid; }

  _flushPid() {
      if (this.pid === null) {
        return;
      }
      this.exitStatus = null;
      this.pid = null;
      this.upAndRunning = false;
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
      agencyConfig: this.agencyMgr.getStructure(),
      upAndRunning: this.upAndRunning,
      suspended: this.suspended,
      port: this.port,
      url: this.url,
      endpoint: this.endpoint,
      dataDir: this.dataDir,
      appDir: this.appDir,
      tmpDir: this.tmpDir,
      logFile: this.logFile,
      args: this.args,
      pid: this.pid,
      id: this.id,
      shortName: this.shortName,
      JWT: this.JWT,
      jwtFiles: this.jwtFiles,
      exitStatus: this.exitStatus,
      serverCrashedLocal: this.serverCrashedLocal
    };
  }

  setFromStructure(struct, agencyMgr) {
    this.name = struct['name'];
    this.instanceRole = struct['instanceRole'];
    this.message = struct['message'];
    this.rootDir = struct['rootDir'];
    this.protocol = struct['protocol'];
    this.authHeaders = struct['authHeaders'];
    this.restKeyFile = struct['restKeyFile'];
    this.upAndRunning = struct['upAndRunning'];
    this.suspended = struct['suspended'];
    this.port = struct['port'];
    this.url = struct['url'];
    this.endpoint = struct['endpoint'];
    this.dataDir = struct['dataDir'];
    this.appDir = struct['appDir'];
    this.tmpDir = struct['tmpDir'];
    this.logFile = struct['logFile'];
    this.args = struct['args'];
    this.pid = struct['pid'];
    this.id = struct['id'];
    this.shortName = struct['shortName'];
    this.JWT = struct['JWT'];
    this.jwtFiles = struct['jwtFiles'];
    this.exitStatus = struct['exitStatus'];
    this.serverCrashedLocal = struct['serverCrashedLocal'];
  }

  setThisConnectionHandle() {
    this.connectionHandle = arango.getConnectionHandle();
  }
  isRole(compareRole) {
    // print(this.instanceRole + ' ==? ' + compareRole);
    return this.instanceRole === compareRole;
  }

  isAgent() {
    return this.instanceRole === instanceRole.agent;
  }
  isFrontend() {
    return ( (this.instanceRole === instanceRole.single) ||
             (this.instanceRole === instanceRole.coordinator) ||
             (this.instanceRole === instanceRole.failover)      );
  }

  matches(role, urlIDOrShortName) {
    if (role !== undefined && role !== '' && !this.isRole(role)) {
      return false;
    }

    if (urlIDOrShortName !== undefined &&
        this.url !== urlIDOrShortName &&
        this.shortName !== urlIDOrShortName &&
        this.endpoint !== urlIDOrShortName &&
        this.id !== urlIDOrShortName) {
      return false;
    }
    return true;
  }
  
  _disconnect() {
    if (this.connectionHandle !== undefined) {
      arango.disconnectHandle(this.connectionHandle);
    }
    this.connectionHandle = undefined;
  }

  resetAuthHeaders(authHeaders, JWT) {
    this.authHeaders = authHeaders;
    this.JWT = JWT;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief arguments for testing (server)
  // //////////////////////////////////////////////////////////////////////////////

  _makeArgsArangod () {
    if (this.options.hasOwnProperty('dummy')) {
      return;
    }
    console.assert(this.tmpDir !== undefined);
    let endpoint;
    let bindEndpoint;
    if (!this.args.hasOwnProperty('server.endpoint')) {
      this.port = this.pm.findFreePort(this.options.minPort, this.options.maxPort);
      this.endpoint = this.protocol + '://127.0.0.1:' + this.port;
      bindEndpoint = this.endpoint;
      if (this.options.bindBroadcast) {
        bindEndpoint = this.protocol + '://0.0.0.0:' + this.port;
      }
    } else {
      this.endpoint = this.args['server.endpoint'];
      bindEndpoint = this.endpoint;
      this.port = this.endpoint.split(':').pop();
    }
    this.url = pu.endpointToURL(this.endpoint);

    if (this.appDir === undefined) {
      this.appDir = fs.getTempPath();
    }

    let config = 'arangod-' + this.instanceRole + '.conf';
    this.args = _.defaults(this.args, {
      'configuration': fs.join(pu.CONFIG_DIR, config),
      'define': 'TOP_DIR=' + pu.TOP_DIR,
      'javascript.app-path': this.appDir,
      'javascript.copy-installation': false,
      'http.trusted-origin': this.options.httpTrustedOrigin || 'all',
      'temp.path': this.tmpDir,
      'server.endpoint': bindEndpoint,
      'database.directory': this.dataDir,
      'temp.intermediate-results-path': fs.join(this.rootDir, 'temp-rocksdb-dir'),
      'log.file': this.logFile
    });
    if (this.options.forceOneShard) {
      this.args['cluster.force-one-shard'] = true;
    }
    if (require("@arangodb/test-helper").isEnterprise()) {
      this.args['arangosearch.columns-cache-limit'] = '100000';
    }
    if (this.options.auditLoggingEnabled) {
      this.args['audit.output'] = 'file://' + fs.join(this.rootDir, 'audit.log');
      this.args['server.statistics'] = false;
      this.args['foxx.queues'] = false;
    }

    if (this.protocol === 'ssl' && !this.args.hasOwnProperty('ssl.keyfile')) {
      this.args['ssl.keyfile'] = fs.join('etc', 'testing', 'server.pem');
    }
    if (this.options.encryptionAtRest && !this.args.hasOwnProperty('rocksdb.encryption-keyfile')) {
      this.args['rocksdb.encryption-keyfile'] = this.restKeyFile;
    }

    if (this.restKeyFile && !this.args.hasOwnProperty('server.jwt-secret')) {
      this.args['server.jwt-secret'] = this.restKeyFile;
    }
    else if (this.options.hasOwnProperty('jwtFiles')) {
      this.jwtFiles = this.options['jwtFiles'];
      // instanceInfo.authOpts['server.jwt-secret-folder'] = addArgs['server.jwt-secret-folder'];
      this.jwtFiles.forEach(file => {
        this.jwtSecrets.push(fs.read(file));
      });
    }
    if (this.jwtSecrets.length > 0) {
      this.JWT = this.jwtSecrets[0];
    }

    if (this.options.hasOwnProperty("replicationVersion")) {
      this.args['database.default-replication-version'] = this.options.replicationVersion;
    }

    for (const [key, value] of Object.entries(this.options.extraArgs)) {
      let splitkey = key.split('.');
      if (splitkey.length !== 2) {
        if (splitkey[0] === this.instanceRole) {
          this.args[splitkey.slice(1).join('.')] = value;
        }
      } else {
        this.args[key] = value;
      }
    }

    let output = this.args.hasOwnProperty('log.output') ? this.args['log.output'] : [];
    if (typeof output === 'string') {
      output = [output];
    }

    if (this.options.verbose) {
      this.args['log.level'] = 'debug';
      output.push('-'); // make sure we always have a stdout appender
    } else if (this.options.noStartStopLogs) {
      // set the stdout appender to error only
      output.push('-;all=error');
      let logs = ['crash=info'];
      if (this.args['log.level'] !== undefined) {
        if (Array.isArray(this.args['log.level'])) {
          logs = logs.concat(this.args['log.level']);
        } else {
          logs.push(this.args['log.level']);
        }
      }
      this.args['log.level'] = logs;
    } else {
      output.push('-'); // make sure we always have a stdout appender
    }
    this.args['log.output'] = output;
    if (this.isAgent()) {
      this.args = Object.assign(this.args, {
        'agency.activate': 'true',
        'agency.size': this.agencyMgr.agencySize,
        'agency.wait-for-sync': this.agencyMgr.waitForSync,
        'agency.supervision': this.agencyMgr.supervision,
        'agency.my-address': this.protocol + '://127.0.0.1:' + this.port,
        // Sometimes for unknown reason the agency startup is too slow.
        // With this log level we might have a chance to see what is going on.
        'log.level': "agency=info",
      });
      if (!this.args.hasOwnProperty("agency.supervision-grace-period")) {
        this.args['agency.supervision-grace-period'] = '10.0';
      }
      if (!this.args.hasOwnProperty("agency.supervision-frequency")) {
        this.args['agency.supervision-frequency'] = '1.0';
      }
      this.agencyMgr.agencyInstances.push(this);
      if (this.agencyMgr.agencyInstances.length === this.agencyMgr.agencySize) {
        let l = [];
        this.agencyMgr.agencyInstances.forEach(agentInstance => {
          l.push(agentInstance.endpoint);
          this.agencyMgr.urls.push(agentInstance.url);
          this.agencyMgr.endpoints.push(agentInstance.endpoint);
        });
        this.agencyMgr.agencyInstances.forEach(agentInstance => {
          agentInstance.args['agency.endpoint'] = _.clone(l);
        });
        this.agencyMgr.agencyEndpoint = this.agencyMgr.agencyInstances[0].endpoint;
      }
    } else if (this.instanceRole === instanceRole.dbServer) {
      this.args = Object.assign(this.args, {
        'cluster.my-role':'PRIMARY',
        'cluster.my-address': this.args['server.endpoint'],
        'cluster.agency-endpoint': this.agencyMgr.agencyEndpoint
      });
    } else if (this.instanceRole === instanceRole.coordinator) {
      this.args = Object.assign(this.args, {
        'cluster.my-role': 'COORDINATOR',
        'cluster.my-address': this.args['server.endpoint'],
        'cluster.agency-endpoint': this.agencyMgr.agencyEndpoint,
        'foxx.force-update-on-startup': true
      });
      if (!this.args.hasOwnProperty('cluster.default-replication-factor')) {
        this.args['cluster.default-replication-factor'] = '2';
      }
    }
    if (this.options.isInstrumented && this.instanceRole in [
      instanceRole.dbServer,
      instanceRole.coordinator,
      instanceRole.agent]) {
      this.args = Object.assign(this.args, {
        'replication.connect-timeout':  20,
      });
    }
    if (this.options.forceNoCompress) {
      this.args = Object.assign(this.args, {
        'http.compress-response-threshold':  99999999999,
      });
    }
    if (this.args.hasOwnProperty('server.jwt-secret')) {
      this.JWT = this.args['server.jwt-secret'];
    }
    this.sanHandler.detectLogfiles(this.rootDir, this.topLevelTmpDir);
  }
  
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief make this instance an issue of the past.
  // //////////////////////////////////////////////////////////////////////////////
  cleanup() {
    if ((this.pid !== null) && (this.exitStatus === null)) {
      print(RED + Date() + "killing instance (again?) to make sure we can delete its files!" + RESET);
      this.terminateInstance();
    }
    if (this.options.extremeVerbosity) {
      print(CYAN + "cleaning up " + this.name + " 's Directory: " + this.rootDir + RESET);
    }
    if (fs.exists(this.rootDir)) {
      fs.removeDirectoryRecursive(this.rootDir, true);
    }
  }

  terminateInstance() {
    internal.removePidFromMonitor(this.pid);
    if (!this.hasOwnProperty('exitStatus')) {
      this.exitStatus = killExternal(this.pid, termSignal);
    }
    this.processSanitizerReports();
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief executes a command, possible with valgrind
  // //////////////////////////////////////////////////////////////////////////////

  _executeArangod (moreArgs) {
    if (moreArgs && moreArgs.hasOwnProperty('server.jwt-secret')) {
      this.JWT = moreArgs['server.jwt-secret'];
    }

    let cmd = pu.ARANGOD_BIN;
    let args = _.defaults(moreArgs, this.args);
    let argv = [];
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
      argv = toArgv(valgrindOpts, true).concat([cmd]).concat(toArgv(args));
      cmd = this.options.valgrind;
    } else if (this.options.rr) {
      argv = [cmd].concat(toArgv(args));
      cmd = 'rr';
    } else {
      argv = toArgv(args);
    }

    if (this.options.extremeVerbosity) {
      print(Date() + ' starting process ' + cmd + ' with arguments: ' + JSON.stringify(argv));
    }

    let subEnv = this.sanHandler.getSanOptions();

    if ((this.useableMemory === undefined) && (this.options.memory !== undefined)){
      throw new Error(`${this.name} don't have planned memory though its configured!`);
    }
    if ((this.useableMemory !== 0) && (this.options.memory !== undefined)) {
      if (this.options.extremeVerbosity) {
        print(`appointed ${this.name} memory: ${this.useableMemory}`);
      }
      subEnv.push(`ARANGODB_OVERRIDE_DETECTED_TOTAL_MEMORY=${this.useableMemory}`);
    }
    subEnv.push(`ARANGODB_SERVER_DIR=${this.rootDir}`);
    let ret = executeExternal(cmd, argv, false, subEnv);
    return ret;
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief starts an instance
  // /
  // //////////////////////////////////////////////////////////////////////////////

  startArango () {
    this._disconnect();
    try {
      this.pid = this._executeArangod().pid;
      if (this.options.enableAliveMonitor) {
        internal.addPidToMonitor(this.pid);
      }
    } catch (x) {
      print(`${RED}${Date()} failed to run arangod - ${x.message}\n${x.stack}${RESET}`);
      throw x;
    }
  }

  launchInstance(moreArgs) {
    if (this.pid !== null) {
      print(`${RED}can not re-launch when PID still there. {this.name} - ${this.pid}${RESET}`);
      throw new Error("kill the instance before relaunching it!");
      return;
    }
    this._disconnect();
    try {
      let args = {...this.args, ...moreArgs};
      this.pid = this._executeArangod(args).pid;
    } catch (x) {
      print(`${RED}${Date()} failed to run arangod - ${x.message} - ${JSON.stringify(this.getStructure())}`);
      throw x;
    }
    this.endpoint = this.args['server.endpoint'];
    this.url = pu.endpointToURL(this.endpoint);
  };
  restartOneInstance(moreArgs, unAuthOK) {
    if (unAuthOK === undefined) {
      unAuthOK = false;
    }
    this.moreArgs = moreArgs;
    if (moreArgs && moreArgs.hasOwnProperty('server.jwt-secret')) {
      this.JWT = moreArgs['server.jwt-secret'];
    }
    const startTime = time();
    this.exitStatus = null;
    this.pid = null;
    this.upAndRunning = false;
    this._disconnect();
    
    print(CYAN + Date()  + " relaunching: " + this.name + ', url: ' + this.url + RESET);
    this.launchInstance(moreArgs);
    this.pingUntilReady(this.authHeaders, time() + seconds(60));
    print(CYAN + Date() + ' ' + this.name + ', url: ' + this.url + ', running again with PID ' + this.pid + RESET);
  }

  restartIfType(instanceRoleFilter, moreArgs) {
    if (this.pid || this.instanceRole !==  instanceRoleFilter) {
      return true;
    }
    this.restartOneInstance(moreArgs);
  }

  status(waitForExit) {
    let ret = statusExternal(this.pid, waitForExit);
    if (ret.status !== 'RUNNING') {
      this.processSanitizerReports();
      this._disconnect();
    }
    return ret;
  }

  isRunning() {
    let check = () => (this.exitStatus !== null) && (this.exitStatus.status === 'RUNNING');
    if (this.exitStatus === null || check()) {
      this.exitStatus = this.status(false);
      return check();
    }
    return false;
  }

  runUpgrade() {
    let moreArgs = {
      '--database.auto-upgrade': 'true',
      '--log.foreground-tty': 'true'
    };
    if (this.role === instanceRole.coordinator) {
      moreArgs['--server.rest-server'] = 'false';
    }
    this.exitStatus = null;
    this.pid = this._executeArangod(moreArgs).pid;
    sleep(1);
    while (this.isRunning()) {
      print(".");
      sleep(1);
    }
    print(`${Date()} upgrade of ${this.name} finished.`);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief periodic checks whether spawned arangod processes are still alive
  // //////////////////////////////////////////////////////////////////////////////
  checkArangoAlive () {
    if (this.pid === null) {
      return false;
    }
    let res = statusExternal(this.pid, false);
    if (res.status === 'NOT-FOUND') {
      print(`${Date()} ${this.name}: PID ${this.pid} missing on our list, retry?`);
      sleep(0.2);
      res = statusExternal(this.pid, false);
    }
    const running = res.status === 'RUNNING';
    if (!this.options.coreCheck && this.options.setInterruptable && !running) {
      print(`fatal exit of ${this.pid} arangod => ${JSON.stringify(res)}! Bye!`);
      pu.killRemainingProcesses({status: false});
      throw new Error(`fatal exit of ${this.pid} arangod => ${JSON.stringify(res)}! Bye!`);
    }

    if (!running) {
      if (!this.hasOwnProperty('message')) {
        this.message = '';
      }
      let msg = ` ArangoD of role [${this.name}] with PID ${this.pid} is gone by: ${JSON.stringify(res)}`;
      print(Date() + msg + ':');
      this.message += (this.message.length === 0) ? '\n' : '' + msg + ' ';
      if (!this.hasOwnProperty('exitStatus') || (this.exitStatus === null)) {
        this.exitStatus = res;
      }

      if (res.hasOwnProperty('signal') &&
          ((res.signal === 11) ||
           (res.signal === 6))) {
        msg = 'health Check Signal(' + res.signal + ') ';
        this.analyzeServerCrash(msg);
        this.serverCrashedLocal = true;
        this.message += msg;
        msg = " checkArangoAlive: Marking crashy";
        pu.serverCrashed = true;
        this.message += msg;
        print(Date() + msg + ' - ' + JSON.stringify(this.getStructure()));
        this.pid = null;
      }
    }
    return running;
  }

  checkServerGood(health) {
    let name = '';
    name = "on node " + this.name;
    if (this.isAgent() || this.isRole(instanceRole.single)) {
      return [name, true];
    }
    if (health.hasOwnProperty(this.id)) {
      this.shortName = health[this.id].ShortName;
      if (health[this.id].Status === "GOOD") {
        return [name, true];
      } else {
        print(`${RED}ClusterHealthCheck failed ${this.id} has status ${health[this.id].Status} (which is not equal to GOOD)${RESET}`);
        return [name, false];
      }
    } else {
      print(`${RED}ClusterHealthCheck failed ${this.id} does not have health property${RESET}`);
      return [name, false];
    }
  }

  pingUntilReady(httpAuthOptions, deadline) {
    if (this.suspended) {
      return;
    }
    let httpOptions = _.clone(httpAuthOptions);
    httpOptions.method = 'POST';
    httpOptions.returnBodyOnError = true;
    while (true) {
      wait(1, false);
      try {
        if (true) {//if (this.options.useReconnect && this.isFrontend()) {
          if (this.JWT) {
            print(`${Date()} reconnecting with JWT ${this.JWT} to ${this.url}`);
            if (arango.reconnect(this.endpoint,
                                 '_system',
                                 `${this.options.username}`,
                                 this.options.password,
                                 true,
                                 this.JWT)) {
              this.connectionHandle = arango.getConnectionHandle();
            }
          } else {
            print(`${Date()} ${this.name} reconnecting ${this.url}`);
            if (arango.reconnect(this.endpoint,
                                 '_system',
                                 `${this.options.username}`,
                                 this.options.password,
                                 true)) {
              this.connectionHandle = arango.getConnectionHandle();
            }
          }
          return;
        } else {
          print(`${Date()} tickeling ${this.url}`);
          const reply = download(this.url + '/_api/version', '', httpOptions);
          if (!reply.error && reply.code === 200) {
            return;
          }
        }
      } catch (ex) {
        if (time() < deadline) {
          print('.');
          sleep(1);
        } else {
          throw new Error(`server did not become availabe on time : ${deadline} - ${ex.message}`);
        }
      }

      if (time() <= deadline) {
        if (!this.checkArangoAlive()) {
          throw new Error('startup failed! bailing out!');
        }
        print('.');
      } else {
        throw new Error(`server did not become availabe on time : ${deadline}`);
      }
    }
  }

  connect() {
    if (this.connectionHandle !== undefined) {
      if (this.connectionHandle === arango.getConnectionHandle()) {
        return true;
      }
      try {
        return arango.connectHandle(this.connectionHandle);
      } catch (ex) {
        print(`${this.name}: Connection ${this.connectionHandle} not found, continuing with regular connection: ${ex}\n${ex.stack}`);
        this.connectionHandle = undefined;
      }
    }
    if (this.JWT) {
      print(`${Date()} ${this.name}: re/connecting with JWT ${this.url}`);
      const ret = arango.reconnect(this.endpoint, '_system', 'root', '', true, this.JWT);
      this.connectionHandle = arango.getConnectionHandle();
      return ret;
    } else {
      print(`${Date()} ${this.name} re/connecting ${this.url}`);
      const ret = arango.reconnect(this.endpoint, '_system', 'root', '', true);
      this.connectionHandle = arango.getConnectionHandle();
      return ret;
    }
  }
  checkArangoConnection(count, overrideVerbosity=false) {
    this.endpoint = this.args['server.endpoint'];
    while (count > 0) {
      try {
        if (this.options.extremeVerbosity || overrideVerbosity) {
          print(`${Date()} ${this.name}: tickeling ${this.endpoint}`);
        }
        this.connect();
        return;
      } catch (e) {
        if (this.options.extremeVerbosity || overrideVerbosity) {
          print(`${this.name}:  no... ${e.message}`);
        }
        this._disconnect();
        sleep(0.5);
      }
      count --;
    }
    throw new Error(`${this.name}:  unable to connect in ${count}s`);
  }

  waitForExitAfterDebugKill() {
    if (this.pid === null) {
      return;
    }
    // Crashutils debugger kills our instance, but we neet to get
    // testing.js spawned-PID-monitoring adjusted.
    print(`${this.name}:  waiting for exit - ${this.pid}`);
    try {
      let ret = statusExternal(this.pid, false);
      // OK, something has gone wrong, process still alive. announce and force kill:
      if (ret.status !== "ABORTED") {
        print(`${RED}was expecting the ${this.name} process ${this.pid} to be gone, but ${JSON.stringify(ret)}${RESET}`);
        this.processSanitizerReports();
        killExternal(this.pid, abortSignal);
        print(statusExternal(this.pid, true));
      }
      this._disconnect();
   } catch(ex) {
      print(ex);
    }
    this.pid = null;
    print(`${this.name}: done`);
  }

  waitForExit() {
    if (this.pid === null) {
      this.exitStatus = null;
      return;
    }
    this.exitStatus = statusExternal(this.pid, true);
    this._disconnect();
    if (this.exitStatus.status !== 'TERMINATED') {
      this.processSanitizerReports();
      throw new Error(this.name + " didn't exit in a regular way: " + JSON.stringify(this.exitStatus));
    }
    this.exitStatus = null;
    this.pid = null;
  }

  killWithCoreDump (message) {
    if (this.pid == null) {
      return;
    }
    let pid = this.pid;
    if (this.options.enableAliveMonitor) {
      internal.removePidFromMonitor(this.pid);
    }
    this.getInstanceProcessStatus();
    this.serverCrashedLocal = true;
    if (this.pid === null) {
      this.pid = pid;
      print(`${RED}${Date()} ${this.name}: instance already gone? ${JSON.stringify(this.exitStatus)}${RESET}`);
      this.analyzeServerCrash(`instance ${this.name} during force terminate server already dead? ${JSON.stringify(this.exitStatus)}`);
      this.pid = null;
    } else {
      print(`${RED}${Date()} attempting to generate crashdump of: ${this.name} ${JSON.stringify(this.exitStatus)}${RESET}`);
      crashUtils.generateCrashDump(pu.ARANGOD_BIN, this, this.options, message);
    }
  }

  aggregateDebugger () {
    crashUtils.aggregateDebugger(this, this.options);
    print(CYAN + Date() + this.name + ', url: ' + this.url + "unlisting our instance" + RESET);
    this.waitForExitAfterDebugKill();
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
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief commands a server to shut down via webcall
  // //////////////////////////////////////////////////////////////////////////////

  shutdownArangod (forceTerminate) {
    if (this.pid == null) {
      print(CYAN + Date() + this.name + ', url: ' + this.url + ' already dead, doing nothing' + RESET);
      return;
    }
    print(CYAN + Date() +' stopping ' + this.name + ', pid ' + this.pid + ', url: ' + this.url + ', force terminate: ' + forceTerminate + ' ' + this.protocol + RESET);
    if (forceTerminate === undefined) {
      forceTerminate = false;
    }
    if (this.options.hasOwnProperty('server')) {
      print(`${Date()} ${this.name}: running with external server`);
      return;
    }

    if (this.options.valgrind) {
      this.waitOnServerForGC(60);
    }
    if (this.options.rr && forceTerminate) {
      forceTerminate = false;
      this.options.useKillExternal = true;
    }
    if (this.options.enableAliveMonitor) {
      internal.removePidFromMonitor(this.pid);
    }
    if ((this.exitStatus === null) ||
        (this.exitStatus.status === 'RUNNING')) {
      if (forceTerminate) {
        let sockStat = this.getSockStat(Date() + "Force killing - sockstat before: ");
        this.killWithCoreDump('shutdown timeout; instance forcefully KILLED because of fatal timeout in testrun ' + sockStat);
        this._disconnect();
        this.pid = null;
      } else if (this.options.useKillExternal) {
        let sockStat = this.getSockStat("Shutdown by kill - sockstat before: ");
        this.exitStatus = killExternal(this.pid);
        this._disconnect();
        this.pid = null;
        print(sockStat);
      } else if (this.protocol === 'unix') {
        let sockStat = this.getSockStat("Sock stat for: ");
        let reply = {code: 555};
        try {
          print(this.connect());
          reply = arango.DELETE_RAW('/_admin/shutdown');
        } catch(ex) {
          print(RED + 'while invoking shutdown via unix domain socket: ' + ex + RESET);
        };
        if ((reply.code !== 200) && // if the server should reply, we expect 200 - if not:
            !((reply.code === 500) &&
              (
                (reply.parsedBody === "Connection closed by remote") || // http connection
                  reply.parsedBody.includes('failed with #111')           // https connection
              ))) {
          this.serverCrashedLocal = true;
          print(Date() + ' Wrong shutdown response: ' + JSON.stringify(reply) + "' " + sockStat + " continuing with hard kill!");
          this.shutdownArangod(true);
          this._disconnect();
        } else {
          this.processSanitizerReports();
          if (!this.options.noStartStopLogs) {
            print(sockStat);
          }
        }
        if (this.options.extremeVerbosity) {
          print(Date() + ' Shutdown response: ' + JSON.stringify(reply));
        }
      } else {
        const requestOptions = pu.makeAuthorizationHeaders(this.options, this.args, this.JWT);
        requestOptions.method = 'DELETE';
        requestOptions.timeout = 60; // 60 seconds hopefully are enough for getting a response
        if (!this.options.noStartStopLogs) {
          print(Date() + ' ' + this.url + '/_admin/shutdown');
        }
        let sockStat = this.getSockStat("Sock stat for: ");
        const reply = download(this.url + '/_admin/shutdown', '', requestOptions);
        if ((reply.code !== 200) && // if the server should reply, we expect 200 - if not:
            !((reply.code === 500) &&
              (
                (reply.message === "Connection closed by remote") || // http connection
                  reply.message.includes('failed with #111')           // https connection
              ))) {
          this.serverCrashedLocal = true;
          print(Date() + ' Wrong shutdown response: ' + JSON.stringify(reply) + "' " + sockStat + " continuing with hard kill!");
          this.shutdownArangod(true);
        }
        else if (!this.options.noStartStopLogs) {
          print(sockStat);
        }
        if (this.options.extremeVerbosity) {
          print(Date() + ' Shutdown response: ' + JSON.stringify(reply));
        }
      }
    } else {
      print(Date() + ' Server already dead, doing nothing.');
    }
  }

  waitForInstanceShutdown(timeout) {
    if (this.pid === null) {
      throw new Error(this.name + " already exited!");
    }
    if (this.options.enableAliveMonitor) {
      internal.removePidFromMonitor(this.pid);
    }
    while (timeout > 0) {
      this.exitStatus = statusExternal(this.pid, false);
      if (this.exitStatus.status === 'TERMINATED') {
        this._disconnect();
        return true;
      }
      sleep(1);
      timeout--;
    }
    this.shutDownOneInstance({nonAgenciesCount: 1}, true, 0);
    crashUtils.aggregateDebugger(this, this.options);
    this.waitForExitAfterDebugKill();
    this._disconnect();
    this.pid = null;
    return false;
  }

  shutDownOneInstance(counters, forceTerminate, timeout) {
    let shutdownTime = time();
    if (this.options.enableAliveMonitor) {
      internal.removePidFromMonitor(this.pid);
    }
    if (this.exitStatus === null) {
      this.shutdownArangod(forceTerminate);
      if (forceTerminate) {
        print(Date() + " FORCED shut down: " + JSON.stringify(this.getStructure()));
      } else {
        this.exitStatus = {
          status: 'RUNNING'
        };
        print(Date() + " Commanded shut down: " + JSON.stringify(this.getStructure()));
      }
      return true;
    }
    if (this.exitStatus.status === 'RUNNING') {
      this.exitStatus = statusExternal(this.pid, false);
    }
    if (this.exitStatus.status === 'RUNNING') {
      let localTimeout = timeout;
      if (this.isAgent()) {
        localTimeout = localTimeout + 60;
      }
      if ((time() - shutdownTime) > localTimeout) {
        this.agencyMgr.dumpAgency();
        print(Date() + ' forcefully terminating ' + yaml.safeDump(this.getStructure()) +
              ' after ' + timeout + 's grace period; marking crashy.');
        this.serverCrashedLocal = true;
        counters.shutdownSuccess = false;
        this.killWithCoreDump('shutdown timeout; instance "' +
                              this.name +
                              '" forcefully KILLED after 60s');
        if (!this.isAgent()) {
          counters.nonAgenciesCount--;
        }
        return false;
      } else {
        return true;
      }
    } else if (this.exitStatus.status !== 'TERMINATED') {
      if (!this.isAgent()) {
        counters.nonAgenciesCount--;
      }
      if (this.exitStatus.hasOwnProperty('signal') || this.exitStatus.hasOwnProperty('monitor')) {
        this.analyzeServerCrash('instance "' + this.name + '" Shutdown - ' + this.exitStatus.signal);
        print(Date() + " shutdownInstance: Marking crashy - " + JSON.stringify(this.getStructure()));
        this.serverCrashedLocal = true;
        counters.shutdownSuccess = false;
      }
      crashUtils.stopProcdump(this.options, this);
    } else {
      if (!this.isAgent()) {
        counters.nonAgenciesCount--;
      }
      if (!this.options.noStartStopLogs) {
        print(Date() + ' Server "' + this.name + '" shutdown: Success: pid', this.pid);
      }
      crashUtils.stopProcdump(this.options, this);
      return false;
    }
  }

  getInstanceProcessStatus() {
    if (this.pid !== null) {
      this.exitStatus = statusExternal(this.pid, false);
      if (this.exitStatus.status !== 'RUNNING') {
        this.pid = null;
      }
    }
  }

  suspend() {
    if (this.suspended) {
      print(CYAN + Date() + ' NOT suspending "' + this.name + " again!" + RESET);
      return;
    }
    print(CYAN + Date() + ' suspending ' + this.name + ', url: ' + this.url + RESET);
    if (this.options.enableAliveMonitor) {
      internal.removePidFromMonitor(this.pid);
    }
    if (!suspendExternal(this.pid)) {
      throw new Error("Failed to suspend " + this.name);
    }
    this.suspended = true;
    return true;
  }

  resume() {
    if (!this.suspended) {
      print(CYAN + Date() + ' NOT resuming "' + this.name + " again!" + RESET);
      return;
    }
    print(CYAN + Date() + ' resuming ' + this.name + ', url: ' + this.url + RESET);
    if (!continueExternal(this.pid)) {
      throw new Error("Failed to resume " + this.name);
    }
    if (this.options.enableAliveMonitor) {
      internal.addPidToMonitor(this.pid);
    }
    this.suspended = false;
    return true;
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////                Utility functionality                             ////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief scans the log files for assert lines
  // //////////////////////////////////////////////////////////////////////////////
  readImportantLogLines (logPath) {
    let fnLines = [];
    const buf = fs.readBuffer(fs.join(this.logFile));
    let lineStart = 0;
    let maxBuffer = buf.length;

    for (let j = 0; j < maxBuffer; j++) {
      if (buf[j] === 10) { // \n
        const line = buf.utf8Slice(lineStart, j);
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
    if (this.assertLines.length > 0) {
      this.assertLines.forEach(line => {
        rp.addFailRunsMessage(currentTest, line);
      });
      this.assertLines = [];
    }
    if (this.serverCrashedLocal) {
      rp.addFailRunsMessage(currentTest, this.serverFailMessagesLocal);
      this.serverFailMessagesLocal = "";
    }
  }

  readAssertLogLines (expectAsserts) {
    if (!fs.exists(this.logFile)) {
      if (fs.exists(this.rootDir)) {
        print(`readAssertLogLines: Logfile ${this.logFile} already gone.`);
      }
      return;
    }
    let size = fs.size(this.logFile);
    if (this.options.maxLogFileSize !== 0 && size > this.options.maxLogFileSize) {
      // File bigger 500k? this needs to be a bug in the tests.
      let err=`ERROR: ${this.logFile} is bigger than ${this.options.maxLogFileSize/1024}kB! - ${size/1024} kBytes!`;
      this.assertLines.push(err);
      print(RED + err + RESET);
      return;
    }
    try {
      const buf = fs.readBuffer(this.logFile);
      let lineStart = 0;
      let maxBuffer = buf.length;

      for (let j = 0; j < maxBuffer; j++) {
        if (buf[j] === 10) { // \n
          const line = buf.utf8Slice(lineStart, j);
          lineStart = j + 1;

          // scan for asserts from the crash dumper
          if (line.search('{crash}') !== -1) {
            if (!IS_A_TTY) {
              // else the server has already printed these:
              print("ERROR: " + line);
            }
            this.assertLines.push(line);
            if (!expectAsserts) {
              crashUtils.GDB_OUTPUT += line + '\n';
            }
          }
        }
      }
    } catch (ex) {
      let err="failed to read " + this.logFile + " -> " + ex;
      this.assertLines.push(err);
      print(RED+err+RESET);
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
          const line = ioraw.utf8Slice(lineStart, j);
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
  getProcessStats() {
    this.stats = this._getProcessStats();
  }
  getDeltaProcessStats() {
    let newStats = this._getProcessStats();
    let deltaStats = {};
    for (let key in this.stats) {
      if (key.startsWith('sockstat_')) {
        deltaStats[key] = newStats[key];
      } else {
        deltaStats[key] = newStats[key] - this.stats[key];
      }
    }
    deltaStats[this.pid + '_' + this.instanceRole] = deltaStats;
    this.stats = newStats;
    return deltaStats;
  }
  getSockStat(preamble) {
    if (this.options.getSockStat && (platform === 'linux')) {
      let sockStat = preamble + this.pid + "\n";
      sockStat += getSockStatFile(this.pid);
      return sockStat;
    }
    return "";
  }

  getMemProfSnapshot(opts) {
    let fn = fs.join(this.rootDir, `${this.role}_${this.pid}_${this.memProfCounter}_.heap`);
    let heapdumpReply = download(this.url + '/_admin/status?memory=true', opts);
    if (heapdumpReply.code === 200) {
      fs.write(fn, heapdumpReply.body);
      print(CYAN + Date() + ` Saved ${fn}` + RESET);
    } else {
      print(RED + Date() + ` Acquiring Heapdump for ${fn} failed!` + RESET);
      print(heapdumpReply);
    }

    let fnMetrics = fs.join(this.rootDir, `${this.role}_${this.pid}_${this.memProfCounter}_.metrics`);
    let metricsReply = download(this.url + '/_admin/metrics/v2', opts);
    if (metricsReply.code === 200) {
      fs.write(fnMetrics, metricsReply.body);
      print(CYAN + Date() + ` Saved ${fnMetrics}` + RESET);
    } else if (metricsReply.code === 503) {
      print(RED + Date() + ` Acquiring metrics for ${fnMetrics} not possible!` + RESET);
    } else {
      print(RED + Date() + ` Acquiring metrics for ${fnMetrics} failed!` + RESET);
      print(metricsReply);
    }
    this.memProfCounter ++;
  }

  processSanitizerReports() {
    this.serverCrashedLocal |= this.sanHandler.fetchSanFileAfterExit(this.pid);
  }

  checkNetstat(data) {
    let which = null;
    if (data.local.port === this.port) {
      which = this.netstat['in'];
    } else if (data.remote.port === this.port) {
      which = this.netstat['out'];
    }
    if (which !== null) {
      if (!which.hasOwnProperty(data.state)) {
        which[data.state] = 1;
      } else {
        which[data.state] += 1;
      }
    }
  }

  getProcessInfo(ports) {
    var matchPort = /.*:.*:([0-9]*)/;
    let res = matchPort.exec(this.endpoint);
    if (!res) {
      return "";
    }
    var port = res[1];
    if (this.isAgent()) {
      if (this.options.sniffAgency) {
        ports.push('port ' + port);
      }
    } else if (this.isRole(instanceRole.dbServer)) {
      if (this.options.sniffDBServers) {
        ports.push('port ' + port);
      }
    } else {
      ports.push('port ' + port);
    }
    return `  [${this.name}] up with pid ${this.pid} - ${this.dataDir}`;
  }
  debugGetFailurePoints() {
    this.connect();
    let haveFailAt = arango.GET("/_admin/debug/failat") === true;
    if (haveFailAt) {
      let res = arango.GET_RAW('/_admin/debug/failat/all');
      if (res.code !== 200) {
        throw "Error checking failure points = " + JSON.stringify(res);
      }
      return res.parsedBody;
    }
    return [];
  }
  debugSetFailAt(failurePoint) {
    if (!this.connect()) {
      throw new Error(`${this.name}: failed to connect my instance {JSON.stringify(this.getStructure())}`);
    }
    let reply = arango.PUT_RAW('/_admin/debug/failat/' + failurePoint, '');
    if (reply.code !== 200) {
      throw new Error(`${this.name}: Failed to set ${failurePoint}: ${reply.parsedBody}`);
    }
    return true;
  }
  debugShouldFailAt(failurePoint) {
    throw new Error("not implemented!");
    if (!this.connect()) {
      throw new Error(`${this.name}: failed to connect my instance {JSON.stringify(this.getStructure())}`);
    }
    let reply = arango.PUT_RAW('/_admin/debug/failat/' + failurePoint, '');
    if (reply.code !== 200) {
      throw new Error(`${this.name}: Failed to set ${failurePoint}: ${reply.parsedBody}`);
    }
    return true;
  }
  debugResetRaceControl() {
    if (!this.connect()) {
      throw new Error(`${this.name}: failed to connect my instance {JSON.stringify(this.getStructure())}`);
    }
    let deleteUrl = '/_admin/debug/raceControl';
    let reply;
    let count = 0;
    while (count < 10) {
      try {
        reply = arango.DELETE_RAW(deleteUrl);
        break;
      } catch (ex) {
        count += 1;
        print(`${RED} ${this.name}: failed to reset race control by ${ex}`);
        this._disconnect();
        this.connect();
      }
    }
    if (reply.code !== 200) {
      // we may no longer be able to work on a database as forced by fuerte
      print(`${BLUE}${this.name}: fallback to internal.download to clear race control${RESET}`);
      let httpOptions = _.clone(this.authHeaders);
      httpOptions.method = 'DELETE';
      httpOptions.returnBodyOnError = true;
      const reply = download(deleteUrl, '', httpOptions);
      if (reply.code !== 200) {
        throw new Error(`${this.name}: Failed to remove race control: =>  ${JSON.stringify(reply.parsedBody)}`);
      }
    }
    return true;
  }
  debugClearFailAt(failurePoint) {
    if (!this.connect()) {
      throw new Error(`${this.name}: failed to connect my instance {JSON.stringify(this.getStructure())}`);
    }
    if (failurePoint === "") {
      failurePoint = undefined;
    }
    let deleteUrl = `/_admin/debug/failat/${(failurePoint=== undefined)?'': '/' + failurePoint}`;
    let reply;
    let count = 0;
    while (count < 10) {
      try {
        reply = arango.DELETE_RAW(deleteUrl);
        break;
      } catch (ex) {
        count += 1;
        print(`${RED} ${this.name}: failed to delete failurepoint by ${ex}`);
        this._disconnect();
        this.connect();
      }
    }
    if (reply.code !== 200) {
      // we may no longer be able to work on a database as forced by fuerte
      print(`${BLUE}${this.name}: fallback to internal.download to clear failurepoint${RESET}`);
      let httpOptions = _.clone(this.authHeaders);
      httpOptions.method = 'DELETE';
      httpOptions.returnBodyOnError = true;
      const reply = download(deleteUrl, '', httpOptions);
      if (reply.code !== 200) {
        throw new Error(`${this.name}: Failed to remove FP: '${failurePoint}' =>  ${JSON.stringify(reply.parsedBody)}`);
      }
    }
    return true;
  }
  debugCanUseFailAt() {
    let reply = arango.GET_RAW('/_admin/debug/failat/');
    if (reply.code !== 200) {
      if (reply.code === 401) {
        throw new Error(`${this.name}: Failed to ask for failurepoint: ${reply.parsedBody}`);
      }
      return false;
    }
    return reply.parsedBody === true;   
  }

  checkDebugTerminated(waitForExit) {
    let res = statusExternal(this.pid, waitForExit);
    if (res.status === 'NOT-FOUND') {
      print(`${Date()} ${this.name}: PID ${this.pid} missing on our list, retry?`);
      sleep(0.2);
      res = statusExternal(this.pid, false);
    }
    const running = res.status === 'RUNNING';
    if (!running) {
      // the test may have abortet by itself already, using SIG_ARBRT or SIG_KILL.
      this.exitStatus = res;
      this.pid = null;
      if (res.hasOwnProperty('signal') &&
          (res.signal !== 6)&&(res.signal !== 9)) {
        throw new Error(`unexpected exit signal of ${this.name} - ${JSON.stringify(res)}`);
      }
      return true;
    }
    return false;
  }
  debugTerminate() {
    if (this.pid === null) {
      return;
    }
    if (!this.checkDebugTerminated(false)){
      let reply;
      try {
        this.connect();
        reply = arango.PUT_RAW('/_admin/debug/crash', '');
      } catch(ex) {
        if (ex instanceof ArangoError && (
          (ex.errorNum === internal.errors.ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT.code) ||
            (ex.errorNum === internal.errors.ERROR_BAD_PARAMETER.code))) {
          print(`Terminated instance ${this.name} - ${ex}`);
          return this.checkDebugTerminated(true);
        }
        throw new Error(`Failed to crash ${this.name}: ${ex}`);
      }
      if (reply.code !== 200) {
        throw new Error(`Failed to crash ${this.name}: ${reply.parsedBody}`);
      }
    }
    let count = 0;
    while (count < 10) {
      if (this.checkDebugTerminated(false)) {
        return;
      }
      count += 1;
      sleep(1);
      print('T?');
    }
    throw new Error(`instance wouldn't crash ${this.name}`);
  }
}


exports.instance = instance;
exports.instanceType = instanceType;
exports.instanceRole = instanceRole;
exports.registerOptions = function(optionsDefaults, optionsDocumentation) {
  tu.CopyIntoObject(optionsDefaults, {
    'enableAliveMonitor': true,
    'maxLogFileSize': 500 * 1024,
    'skipLogAnalysis': true,
    'bindBroadcast': false,
    'getSockStat': false,
    'rr': false,
  });

  tu.CopyIntoList(optionsDocumentation, [
    ' SUT instance properties:',
    '   - `enableAliveMonitor`: checks whether spawned arangods disapears or aborts during the tests.',
    '   - `maxLogFileSize`: how big logs should be at max - 500k by default',
    "   - `skipLogAnalysis`: don't try to crawl the server logs",
    '   - `bindBroadcast`: whether to work with loopback or 0.0.0.0',
    '   - `rr`: if set to true arangod instances are run with rr',
    '   - `getSockStat`: on linux collect socket stats before shutdown',
    '   - `replicationVersion`: if set, define the default replication version. (Currently we have "1" and "2")',
  ]);
};
