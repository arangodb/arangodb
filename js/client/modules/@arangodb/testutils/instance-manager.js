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
const internal = require('internal');
const fs = require('fs');
const yaml = require('js-yaml');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const rp = require('@arangodb/testutils/result-processing');
const inst = require('@arangodb/testutils/instance');
const { agencyMgr } = require('@arangodb/testutils/agency');
const crashUtils = require('@arangodb/testutils/crash-utils');
const {versionHas} = require("@arangodb/test-helper");
const crypto = require('@arangodb/crypto');
const ArangoError = require('@arangodb').ArangoError;;
const netstat = require('node-netstat');
/* Functions: */
const {
  SetGlobalExecutionDeadlineTo,
  toArgv,
  executeExternal,
  executeExternalAndWait,
  killExternal,
  statusExternal,
  statisticsExternal,
  clusterHealth,
  base64Encode,
  testPort,
  download,
  errors,
  time,
  wait,
  sleep,
  db,
  platform } = internal;

/* Constants: */
// const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = internal.COLORS.COLOR_YELLOW;
const IS_A_TTY = internal.isATTy();

const termSignal = 15;

const instanceRole = inst.instanceRole;

let instanceCount = 1;
const seconds = x => x * 1000;

class instanceManager {
  constructor(protocol, options, addArgs, testname, tmpDir) {
    this.instanceCount = instanceCount++;
    this.protocol = protocol;
    this.options = options;
    this.addArgs = addArgs;
    this.tmpDir = tmpDir || fs.getTempPath();
    this.rootDir = fs.join(this.tmpDir, testname);
    process.env['ARANGOTEST_ROOT_DIR'] = this.rootDir;
    this.options.agency = this.options.agency || this.options.cluster;
    this.agencyMgr = new agencyMgr(options, this);
    this.instanceRoles = [];
    this.urls = [];
    this.endpoints = [];
    this.endpoint = undefined;
    this.connectedEndpoint = undefined;
    this.connectionHandle = undefined;
    this.arangods = [];
    this.restKeyFile = '';
    this.tcpdump = null;
    this.JWT = null;
    this.dbName = "_System";
    this.userName = "root";
    this.memlayout = {};
    this.cleanup = options.cleanup && options.server === undefined;
    if (!options.hasOwnProperty('startupMaxCount')) {
      this.startupMaxCount = 300;
    } else {
      this.startupMaxCount = options.startupMaxCount;
    }
    if (addArgs.hasOwnProperty('server.jwt-secret')) {
      this.JWT = addArgs['server.jwt-secret'];
    }
    if (this.options.encryptionAtRest) {
      this.restKeyFile = fs.join(this.rootDir, 'openSesame.txt');
      fs.makeDirectoryRecursive(this.rootDir);
      fs.write(this.restKeyFile, "Open Sesame!Open Sesame!Open Ses");
    }
    this.httpAuthOptions = pu.makeAuthorizationHeaders(this.options, addArgs);
    this.expectAsserts = false;
  }

  destructor(cleanup) {
    this.arangods.forEach(arangod => {
      arangod.pm.deregister(arangod.port);
      if (arangod.serverCrashedLocal) {
        cleanup = false;
      }
    });
    this.stopTcpDump();
    if (this.cleanup && cleanup) {
      this._cleanup();
    }
  }
  getStructure() {
    let d = [];
    let ln = "";
    if (this.agencyMgr.leader !== null) {
      ln = this.agencyMgr.leader.name;
    }
    this.arangods.forEach(arangod => { d.push(arangod.getStructure());});
    return {
      protocol: this.protocol,
      options: this.options,
      addArgs: this.addArgs,
      rootDir: this.rootDir,
      leader: ln,
      agencyConfig: this.agencyMgr.getStructure(),
      httpAuthOptions: this.httpAuthOptions,
      urls: this.urls,
      url: this.url,
      endpoints: this.endpoints,
      endpoint: this.endpoint,
      arangods: d,
      restKeyFile: this.restKeyFile,
      tcpdump: this.tcpdump,
      cleanup: this.cleanup,
    };
  }
  setFromStructure(struct) {
    this.arangods = [];
    this.agencyMgr.setFromStructure(struct['agencyConfig']);
    this.protocol = struct['protocol'];
    this.options = struct['options'];
    this.options['dummy'] = true;
    this.addArgs = struct['addArgs'];
    this.rootDir = struct['rootDir'];
    this.httpAuthOptions = struct['httpAuthOptions'];
    this.urls = struct['urls'];
    this.url = struct['url'];
    this.endpoints = struct['endpoints'];
    this.endpoint = struct['endpoint'];
    this.restKeyFile = struct['restKeyFile'];
    this.tcpdump = struct['tcpdump'];
    this.cleanup = struct['cleanup'];
    struct['arangods'].forEach(arangodStruct => {
      let oneArangod = new inst.instance(this.options, '', {}, {}, '', '', '', this.agencyMgr, this.tmpDir);
      oneArangod.setFromStructure(arangodStruct);
      this.arangods.push(oneArangod);
      if (oneArangod.isAgent()) {
        this.agencyMgr.agencyInstances.push(oneArangod);
      }
    });
    let ln = struct['leader'];
    if (ln !== "") {
      this.arangods.forEach(arangod => {
        if (arangod.name === ln) {
          this.agencyMgr.leader = arangod;
        }
      });
    }
    this.arangods.forEach(arangod => {
      if (this.endpoint === arangod.endpoint) {
        arangod.setThisConnectionHandle();
      }
    });
  }
  getTypeToUrlsMap() {
    let ret = new Map();
    this.instanceRoles.forEach(role => {
      ret.set(role, []);
    });
    this.arangods.forEach(arangod => {
      ret.get(arangod.instanceRole).push(arangod.url);
    });
    return ret;
  }
  rememberConnection() {
    this.connectionHandle = arango.getConnectionHandle();
    this.dbName = '_system';
    try {
      this.dbName = db._name();
    } catch (e) {}
    this.userName = arango.connectedUser();
    this.connectedEndpoint = arango.getEndpoint();
    db._useDatabase('_system');
  }
  reconnectMe() {
    if (this.connectionHandle !== undefined) {
      try {
        let ret = arango.connectHandle(this.connectionHandle);
        db._useDatabase(this.dbName);
        return ret;
      } catch (ex) {
        print(`${RED}${Date()} failed to reconnect handle ${this.connectionHandle} ${ex} - trying conventional reconnect.${RESET}`);
      }
    }
    return arango.reconnect(this.connectedEndpoint, this.dbName, this.userName, '');
  }
  debugCanUseFailAt() {
    const res = arango.GET_RAW("_admin/debug/failat");
    if (res.code !== 200) {
      if (res.code === 401) {
        throw `Error asking for failure point.`;
      }
      return false;
    }
    return res.parsedBody === true;
  }
  debugSetFailAt(failurePoint, role, urlIDOrShortName) {
    let count = 0;
    this.rememberConnection();
    this.arangods.forEach(arangod => {
      if (!arangod.matches(role, urlIDOrShortName)) {
        return;
      }
      if (arangod.debugSetFailAt(failurePoint)) {
        count += 1;
      }
    });
    if (count === 0) {
      let msg = "";
      this.arangods.forEach(arangod => {msg += `\n Name => ${arangod.name}  ShortName => ${arangod.shortName} Role=> ${arangod.instanceRole} URL => ${arangod.url} Endpoint: => ${arangod.endpoint}`;});
      throw new Error(`no server matched your conditions to set failurepoint ${failurePoint}, ${urlIDOrShortName}, ${role},${msg}`);
    }
    this.reconnectMe();
  }
  debugShouldFailAt(failurePoint, role, urlIDOrShortName) {
    let count = 0;
    this.rememberConnection();
    this.arangods.forEach(arangod => {
      if (!arangod.matches(role, urlIDOrShortName)) {
        return;
      }
      if (arangod.debugShouldFailAt(failurePoint)) {
        count += 1;
      }
    });
    if (count === 0) {
      let msg = "";
      this.arangods.forEach(arangod => {msg += `\n Name => ${arangod.name}  ShortName => ${arangod.shortName} Role=> ${arangod.instanceRole} URL => ${arangod.url} Endpoint: => ${arangod.endpoint}`;});
      throw new Error(`no server matched your conditions to set failurepoint ${failurePoint}, ${urlIDOrShortName}, ${role},${msg}`);
    }
    this.reconnectMe();
  }
  debugResetRaceControl(role, urlIDOrShortName) {
    this.rememberConnection();
    this.arangods.forEach(arangod => {
      if (!arangod.matches(role, urlIDOrShortName)) {
        return;
      }
      arangod.debugResetRaceControl();
    });
    this.reconnectMe();
  }
  debugRemoveFailAt(failurePoint, role, urlIDOrShortName) {
    this.rememberConnection();
    this.arangods.forEach(arangod => {
      if (!arangod.matches(role, urlIDOrShortName)) {
        return;
      }
      arangod.debugClearFailAt(failurePoint);
    });
    this.reconnectMe();
  }
  debugClearFailAt(failurePoint, role, urlIDOrShortName) {
    this.rememberConnection();
    this.arangods.forEach(arangod => {
      if (!arangod.matches(role, urlIDOrShortName)) {
        return;
      }
      arangod.debugClearFailAt(failurePoint);
    });
    this.reconnectMe();
  }
  debugTerminate() {
    this.arangods.forEach(arangod => {arangod.debugTerminate();});
    return 0;
  }
  checkDebugTerminated() {
    let ret = true;
    this.arangods.forEach(arangod => {ret = arangod.checkDebugTerminated() && ret;});
    return ret;
  }

  _getNames(arangods) {
    let names = [];
    arangods.forEach(arangod => {names.push(arangod.name);});
    return names;
  }

  _cleanup() {
    this.arangods.forEach(instance => { instance.cleanup(); });
    if (this.options.extremeVerbosity) {
      print(CYAN + "cleaning up manager's Directory: " + this.rootDir + RESET);
    }
    fs.removeDirectoryRecursive(this.rootDir, true);
  }

  _getMemLayout () {
    if (this.options.memory !== undefined) {
      if (this.options.cluster) {
        // Distribute 8 % agency, 23% coordinator, 69% dbservers
        this.memlayout[instanceRole.agent] = Math.round(this.options.memory * (8/100) / this.options.agencySize);
        this.memlayout[instanceRole.dbServer] = Math.round(this.options.memory * (69/100) / this.options.dbServers);
        this.memlayout[instanceRole.coordinator] = Math.round(this.options.memory * (23/100) / this.options.coordinators);
      } else if (this.options.agency) {
        this.memlayout[instanceRole.agent] = Math.round(this.options.memory / this.options.agencySize);
      } else {
        this.memlayout[instanceRole.single] = Math.round(this.options.memory / this.options.singles);
      }
    } else {
      // no memory limits
      if (this.options.cluster) {
        this.memlayout[instanceRole.agent] = 0;
        this.memlayout[instanceRole.dbServer] = 0;
        this.memlayout[instanceRole.coordinator] = 0;
      } else if (this.options.agency) {
        this.memlayout[instanceRole.agent] = 0;
      } else {
        this.memlayout[instanceRole.single] = 0;
      }
    }
  }

  prepareInstance () {
    try {
      let frontendCount = 0;
      if (this.options.hasOwnProperty('server')) {
        /// external server, not managed by us.
        this.endpoint = this.options.server;
        this.rootDir = this.options.serverRoot;
        this.url = this.options.server.replace('tcp', 'http');
        arango.reconnect(this.endpoint, '_system', 'root', '');
        return true;
      }
      this._getMemLayout();
      if (this.options.agency) {
        for (let count = 0;
             count < this.agencyMgr.agencySize;
             count ++) {
          this.arangods.push(new inst.instance(this.options,
                                               instanceRole.agent,
                                               this.addArgs,
                                               this.httpAuthOptions,
                                               this.protocol,
                                               fs.join(this.rootDir, instanceRole.agent + "_" + count),
                                               this.restKeyFile,
                                               this.agencyMgr,
                                               this.tmpDir,
                                               this.memlayout[instanceRole.agent]));
        }
        this.instanceRoles.push(instanceRole.agent);
      }

      if (this.options.cluster) {
        for (let count = 0;
             count < this.options.dbServers;
             count ++) {
          this.arangods.push(new inst.instance(this.options,
                                               instanceRole.dbServer,
                                               this.addArgs,
                                               this.httpAuthOptions,
                                               this.protocol,
                                               fs.join(this.rootDir, instanceRole.dbServer + "_" + count),
                                               this.restKeyFile,
                                               this.agencyMgr,
                                               this.tmpDir,
                                               this.memlayout[instanceRole.dbServer]));
        }
        this.instanceRoles.push(instanceRole.dbServer);

        for (let count = 0;
             count < this.options.coordinators;
             count ++) {
          this.arangods.push(new inst.instance(this.options,
                                               instanceRole.coordinator,
                                               this.addArgs,
                                               this.httpAuthOptions,
                                               this.protocol,
                                               fs.join(this.rootDir, instanceRole.coordinator + "_" + count),
                                               this.restKeyFile,
                                               this.agencyMgr,
                                               this.tmpDir,
                                               this.memlayout[instanceRole.coordinator] ));
          frontendCount ++;
        }
        this.instanceRoles.push(instanceRole.coordinator);
      }  else {
        for (let count = 0;
             !this.options.agency && count < this.options.singles;
             count ++) {
          // Single server...
          this.arangods.push(new inst.instance(this.options,
                                               instanceRole.single,
                                               this.addArgs,
                                               this.httpAuthOptions,
                                               this.protocol,
                                               fs.join(this.rootDir, instanceRole.single + "_" + count),
                                               this.restKeyFile,
                                               this.agencyMgr,
                                               this.tmpDir,
                                               this.memlayout[instanceRole.single]));
          this.urls.push(this.arangods[this.arangods.length -1].url);
          this.endpoints.push(this.arangods[this.arangods.length -1].endpoint);
          frontendCount ++;
        }
        this.instanceRoles.push(instanceRole.single);
      }
      if (frontendCount > 0) {
        this.url = this.urls[0];
        this.endpoint = this.endpoints[0];
      } else {
        this.url = null;
        this.endpoint = null;
      };
    } catch (e) {
      print(e, e.stack);
      return false;
    }
  }

  launchInstance() {
    if (this.options.hasOwnProperty('server')) {
      print("external server configured - not testing readyness! " + this.options.server);
      return;
    }
    let subenv = [`INSTANCEINFO=${JSON.stringify(this.getStructure())}`];

    const startTime = time();
    try {
      let count = 0;
      this.arangods.forEach(arangod => {
        arangod.startArango(_.cloneDeep(subenv));
        count += 1;
        this.agencyMgr.detectAgencyAlive(this.httpAuthOptions);
      });
      if (this.options.cluster) {
        this.checkClusterAlive();
      }
      this.launchFinalize(startTime);
      return true;
    } catch (e) {
      // disable watchdog
      let hasTimedOut = SetGlobalExecutionDeadlineTo(0.0);
      if (hasTimedOut) {
        print(RED + Date() + ' Deadline reached! Forcefully shutting down!' + RESET);
      } else {
        print(`${RED}${Date()} Startup failed! Forcefully shutting down:\n${e.message}${e.stack}\n${RESET}`);
      }
      this.arangods.forEach(arangod => {
        try {
          arangod.shutdownArangod(true);
        } catch(e) {
          print("Error cleaning up: ", e, e.stack);
        }
      });
      print(e, e.stack);
      return false;
    }
  }

  restartOneInstance(moreArgs, startTime) {
    /// todo
    if (this.options.cluster && !this.options.skipReconnect) {
      this.checkClusterAlive({}); // todo addArgs
      this.reconnect();
    }
    this.launchFinalize(startTime);
  }


  // //////////////////////////////////////////////////////////////////////////////
  // / @brief scans the log files for important infos
  // //////////////////////////////////////////////////////////////////////////////
  nonfatalAssertSearch() {
    this.expectAsserts = true;
  }
  readImportantLogLines (logPath) {
    let importantLines = {};
    // TODO: iterate over instances
    // for (let i = 0; i < list.length; i++) {
    // }

    return importantLines;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // //////////////////////////////////////////////////////////////////////////////
  // / Server up/down utilities
  // //////////////////////////////////////////////////////////////////////////////
  // //////////////////////////////////////////////////////////////////////////////


  // //////////////////////////////////////////////////////////////////////////////
  // / @brief waits for garbage collection using /_admin/execute
  // //////////////////////////////////////////////////////////////////////////////

  waitOnServerForGC (instanceInfo, options, waitTime) {
    try {
      print(Date() + ' waiting ' + waitTime + ' for server GC');
      const remoteCommand = 'require("internal").wait(' + waitTime + ', true);';
      const requestOptions = pu.makeAuthorizationHeaders(this.options, this.addArgs);
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

  resignLeaderShip(dbServer) {
    let frontend = this.arangods.filter(arangod => {return arangod.isFrontend();})[0];
    print(`${Date()} resigning leaderships from ${dbServer.name} via ${frontend.name}`);
    // make sure the connection is propper:
    frontend._disconnect();
    frontend.connect();

    let result = arango.POST_RAW('/_admin/cluster/resignLeadership',
                                 { "server": dbServer.shortName, "undoMoves": false });
    if (result.code !== 202) {
      throw new Error(`failed to resign ${dbServer.name} from leadership via ${frontend.name}: ${JSON.stringify(result)}`);
    }
    let jobStatus;
    do {
      sleep(1);
      jobStatus = arango.GET_RAW('/_admin/cluster/queryAgencyJob?id=' + result.parsedBody.id);
      print(jobStatus.parsedBody.status);
    } while (jobStatus.parsedBody.status !== 'Finished');
    print(`${Date()} DONE resigning leaderships from ${dbServer.name} via ${frontend.name}`);
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief shuts down an instance
  // //////////////////////////////////////////////////////////////////////////////

  shutdownInstance (forceTerminate, moreReason="") {
    if (forceTerminate === undefined) {
      forceTerminate = false;
    }
    let timeoutReached = SetGlobalExecutionDeadlineTo(0.0);
    if (timeoutReached) {
      print(RED + Date() + ' Deadline reached! Forcefully shutting down!' + RESET);
      moreReason += "Deadline reached! ";
      this.arangods.forEach(arangod => { arangod.serverCrashedLocal = true;});
      forceTerminate = true;
    }
    this.agencyMgr.dumpAgencyIfNotYet(forceTerminate, timeoutReached);
    try {
      if (forceTerminate) {
        return this._forceTerminate(moreReason);
      } else {
        return this._shutdownInstance();
      }
    }
    catch (e) {
      if (e instanceof ArangoError && e.errorNum === errors.ERROR_DISABLED.code) {
        let timeoutReached = SetGlobalExecutionDeadlineTo(0.0);
        if (timeoutReached) {
          print(RED + Date() + ' Deadline reached during shutdown! Forcefully shutting down NOW!' + RESET);
          moreReason += "Deadline reached! ";
        }
        return this._forceTerminate(true, moreReason);
      } else {
        print("caught error during shutdown: " + e);
        print(e.stack);
      }
    }
    return false;
  }

  _forceTerminate(moreReason="") {
    if (!this.options.coreCheck && !this.options.setInterruptable) {
      print("Interactive mode: SIGABRT killing all arangods");
    } else {
      print("Aggregating coredumps");
    }
    this.arangods.forEach((arangod) => {
      arangod.killWithCoreDump('force terminating');
    });
    this.arangods.forEach((arangod) => {
      arangod.aggregateDebugger();
    });
    return true;
  }

  _setMaintenance(onOff) {
    let shutdownSuccess = true;
    try {
      // send a maintenance request to any of the coordinators, so that
      // no failed server/failed follower jobs will be started on shutdown
      let coords = this.arangods.filter(arangod =>
        arangod.isRole(instanceRole.coordinator) &&
          (arangod.exitStatus !== null));
      if (coords.length > 0) {
        let requestOptions = pu.makeAuthorizationHeaders(this.options, this.addArgs);
        requestOptions.method = 'PUT';
        let postBody = onOff ? "on" : "off";
        if (!this.options.noStartStopLogs) {
          print(`${coords[0].url}/_admin/cluster/maintenance => ${postBody}`);
        }
        download(coords[0].url + "/_admin/cluster/maintenance", JSON.stringify(postBody), requestOptions);
      }
    } catch (err) {
      print(`${Date()} error while setting cluster maintenance mode:${err}\n${err.stack}`);
      shutdownSuccess = false;
    }
    return shutdownSuccess;
  }

  _shutdownInstance (moreReason="") {
    let forceTerminate = false;
    let crashed = false;
    let shutdownSuccess = !forceTerminate;

    // we need to find the leading server
    let allAlive = true;
    this.arangods.forEach(arangod => {
      if (arangod.pid === null) {
        allAlive = false;
      }});
    if (!allAlive) {
      return this._forceTerminate("not all instances are alive!");
    }

    if (!forceTerminate && !this.checkInstanceAlive()) {
      print(Date() + ' Server already dead, doing nothing. This shouldn\'t happen?');
    }

    if (!forceTerminate) {
      shutdownSuccess &= this._setMaintenance(true);
    }

    this.stopClusterHealthMonitor();
    // Shut down all non-agency servers:

    let toShutdown = this.arangods.slice().reverse();
    if (!this.options.noStartStopLogs) {
      print(Date() + ' Shutdown order: \n' + yaml.safeDump(this._getNames(toShutdown)));
    }
    let nonAgenciesCount = this.arangods.filter(arangod => {
      if ((arangod.exitStatus !== null) &&
          (arangod.exitStatus.status !== 'RUNNING')) {
        return false;
      }
      return !arangod.isAgent();
    }).length;
    let timeout = 666;
    if (this.options.valgrind) {
      timeout *= 10;
    }
    if (this.options.sanitizer) {
      timeout *= 2;
    }

    if (toShutdown.length > 0) {
      this.agencyMgr.dumpAgency();
    }
    if (forceTerminate) {
      return this._forceTerminate(moreReason);
    }

    var shutdownTime = time();

    while (toShutdown.length > 0) {
      toShutdown = toShutdown.filter(arangod => {
        if (arangod.exitStatus === null) {
          if ((nonAgenciesCount > 0) && arangod.isAgent()) {
            return true;
          }
          arangod.shutdownArangod(false);
          arangod.exitStatus = {
            status: 'RUNNING'
          };

          if (!this.options.noStartStopLogs) {
            print(Date() + " Commanded shut down: " + arangod.name);
          }
          return true;
        }
        if (arangod.isRunning()) {
          let localTimeout = timeout;
          if (arangod.isAgent()) {
            localTimeout = localTimeout + 60;
          }
          if ((time() - shutdownTime) > localTimeout) {
            print(Date() + ' forcefully terminating ' + yaml.safeDump(arangod.getStructure()) +
                  ' after ' + timeout + 's grace period; marking crashy.');
            this.agencyMgr.dumpAgency();
            arangod.serverCrashedLocal = true;
            shutdownSuccess = false;
            arangod.killWithCoreDump(`taking coredump because of timeout ${time()} - ${shutdownTime}) > ${localTimeout} during shutdown`);
            crashUtils.aggregateDebugger(arangod, this.options);
            crashed = true;
            if (!arangod.isAgent()) {
              nonAgenciesCount--;
            }
            return false;
          } else {
            return true;
          }
        } else if ((arangod.exitStatus !== null) && (arangod.exitStatus.status !== 'TERMINATED')) {
          if (!arangod.isAgent()) {
            nonAgenciesCount--;
          }
          if (arangod.exitStatus.hasOwnProperty('signal') || arangod.exitStatus.hasOwnProperty('monitor')) {
            arangod.analyzeServerCrash('instance "' + arangod.name + '" Shutdown - ' + arangod.exitStatus.signal);
            print(Date() + " shutdownInstance: Marking crashy - " + JSON.stringify(arangod.getStructure()));
            arangod.serverCrashedLocal = true;
            shutdownSuccess = false;
          }
        } else {
          if (!arangod.isAgent()) {
            nonAgenciesCount--;
          }
          if (!this.options.noStartStopLogs) {
            print(Date() + ' Server "' + arangod.name + '" shutdown: Success: pid', arangod.pid);
          }
          return false;
        }
      });
      if (toShutdown.length > 0) {
        this.printStillRunningProcessInfo();
        wait(1, false);
      }
    }
    if (!this.options.skipLogAnalysis) {
      this.arangods.forEach(arangod => {
        let errorEntries = arangod.readImportantLogLines();
        if (Object.keys(errorEntries).length > 0) {
          print(Date() + ' Found messages in the server logs: \n' +
                yaml.safeDump(errorEntries));
        }
      });
    }
    this.arangods.forEach(arangod => {
      arangod.readAssertLogLines(this.expectAsserts);
      if (arangod.exitStatus && arangod.exitStatus.exit !== 0) {
        print(RED + `arangod "${arangod.instanceRole}" with pid ${arangod.pid} exited with exit code ${arangod.exitStatus.exit}` + RESET);
        shutdownSuccess = false;
      }
    });
    this.cleanup = this.cleanup && shutdownSuccess;
    return shutdownSuccess;
  }
  launchFinalize(startTime) {
    {//if (!this.options.cluster && !this.options.agency) {
      let deadline = time() + seconds(this.startupMaxCount);
      this.arangods.forEach(arangod => {
        try {
          arangod.pingUntilReady(this.httpAuthOptions, deadline);
        } catch (e) {
          this.arangods.forEach( arangod => {
            let status = arangod.status(false);
            if (status.status !== "RUNNING") {
              throw new Error(`Arangod ${arangod.pid} exited instantly! Status: ${JSON.stringify(status)} ${e}\n${e.stack}\n${JSON.stringify(arangod.getStructure())}`);
            }
          });
          print(Date() + " caught exception: " + e.message);
        }
      });
    }
    this.setEndpoints();
    this.printProcessInfo(startTime);
    sleep(this.options.sleepBeforeStart);
    this.spawnClusterHealthMonitor();
    if (this.options.cluster) {
      this.reconnect();
      this._checkServersGOOD();
    }
  }

  reStartInstance(moreArgs) {
    if (moreArgs === undefined) {
      moreArgs = {};
    }
    const startTime = time();
    this.addArgs = _.defaults(this.addArgs, moreArgs);
    this.httpAuthOptions = pu.makeAuthorizationHeaders(this.options, this.addArgs);
    if (moreArgs.hasOwnProperty('server.jwt-secret')) {
      this.JWT = moreArgs['server.jwt-secret'];
    }

    this.arangods.forEach(arangod => {
      arangod._flushPid();
      arangod.resetAuthHeaders(this.httpAuthOptions, this.JWT);
    });

    let success = true;
    this.instanceRoles.forEach(instanceRole  => {
      this.arangods.forEach(arangod => {
        arangod.restartIfType(instanceRole, moreArgs);
        this.agencyMgr.detectAgencyAlive(this.httpAuthOptions);
      });
    });
    this.launchFinalize(startTime);
  }

  upgradeCycleInstance(waitForShardsInSync) {
    /// TODO:             self._check_for_shards_in_sync()
    this._setMaintenance(true);

    this.instanceRoles.forEach(role => {
      this.arangods.forEach(arangod => {
        if (arangod.isRole(role)) {
          print(`${Date()} upgrading ${arangod.name}`);
          print(`${Date()} stopping ${arangod.name}`);
          if (arangod.isRole(instanceRole.dbServer)) {
            this.resignLeaderShip(arangod);
            sleep(30); // BTS-1965
          }
          arangod.shutdownArangod(false);
          while (arangod.isRunning()) {
            print(".");
            sleep(1);
          }
          print(`${Date()} upgrading ${arangod.name}`);
          arangod.runUpgrade();
          print(`${Date()} relaunching ${arangod.name}`);
          arangod.restartOneInstance();
        }
      });
      if (role === instanceRole.agent) {
        print("running agency health check");
        this.agencyMgr.detectAgencyAlive(this.httpAuthOptions);
      }
    });

    this._setMaintenance(false);
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief check SUT health inbetween
  // //////////////////////////////////////////////////////////////////////////////

  detectShouldBeRunning() {
    let ret = true;
    this.arangods.forEach(arangod => { ret = ret && arangod.pid !== null; } );
    return ret;
  }

  checkUptime () {
    let ret = {};
    let opts = Object.assign(pu.makeAuthorizationHeaders(this.options, this.addArgs),
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

  _checkServersGOOD() {
    let name = '';
    try {
      name = "global health check";
      const health = clusterHealth();
      return this.arangods.every((arangod) => {
        let ret = arangod.checkServerGood(health);
        name += ret[0];
        return ret[1];
      });
    } catch(e) {
      print(`${RED} Error checking cluster health '${name}' => '${e}'\n${e.stack}\n${arango.getEndpoint()}${RESET}`);
      return false;
    }
    return true;
  }

  // skipHealthCheck can be set to true to avoid a call to /_admin/cluster/health
  // on the coordinator. This is necessary if only the agency is running yet.
  checkInstanceAlive({skipHealthCheck = false} = {}) {
    this.arangods.forEach(arangod => { arangod.netstat = {'in':{}, 'out': {}};});
    let obj = this;
    try {
      netstat({platform: process.platform}, function (data) {
        // skip server ports, we know what we bound.
        if (data.state !== 'LISTEN') {
          obj.arangods.forEach(arangod => arangod.checkNetstat(data));
        }
      });
      if (!this.options.noStartStopLogs) {
        this.printNetstat();
      }
    } catch (ex) {
      let timeout = SetGlobalExecutionDeadlineTo(0.0);
      let moreReason = ": " + ex.message;
      if (timeout) {
        moreReason = "because of :" + ex.message;
      }
      print(RED + 'netstat gathering has thrown: ' + moreReason);
      print(ex, ex.stack);
      print(RESET);
      this.cleanup = false;
      return this._forceTerminate("Abort during Health Check SUT netstat gathering " + moreReason);
    }

    let rc = this.arangods.reduce((previous, arangod) => {
      let ret = arangod.checkArangoAlive();
      if (!ret) {
        this.message += arangod.message;
      }
      return previous && ret;
    }, true);
    if (rc && this.options.cluster && !skipHealthCheck && this.arangods.length > 1) {
      const checkAllAlive = () => this.arangods.every(arangod => arangod.checkArangoAlive());
      let first = true;
      rc = false;
      for (
        const start = Date.now();
        !rc && Date.now() < start + seconds(60) && checkAllAlive();
      ) {
        this.reconnect();
        rc = this._checkServersGOOD();
        if (first) {
          if (!this.options.noStartStopLogs) {
            print(RESET + "Waiting for all servers to go GOOD...");
          }
          first = false;
        }
        if (rc) {
          break;
        } else {
          sleep(1);
        }
      }
    }
    if (!rc) {
      this.agencyMgr.dumpAgency();
      print(Date() + ' If cluster - will now start killing the rest.');
      this.arangods.forEach((arangod) => {
        print(Date() + " Killing in the name of: ");
        arangod.killWithCoreDump("health check failed, aborting everything");
      });
      this.arangods.forEach((arangod) => {
        crashUtils.aggregateDebugger(arangod, this.options);
        arangod.waitForExitAfterDebugKill();
      });
    }
    return rc;
  }

  checkClusterAlive() {
    let httpOptions = _.clone(this.httpAuthOptions);
    httpOptions.returnBodyOnError = true;

    // scrape the jwt token
    //instanceInfo.authOpts = _.clone(this.options);
    //if (addArgs['server.jwt-secret'] && !instanceInfo.authOpts['server.jwt-secret']) {
    //  instanceInfo.authOpts['server.jwt-secret'] = addArgs['server.jwt-secret'];
    //} else if (addArgs['server.jwt-secret-folder'] && !instanceInfo.authOpts['server.jwt-secret-folder']) {
    //  instanceInfo.authOpts['server.jwt-secret-folder'] = addArgs['server.jwt-secret-folder'];
    //}


    let count = 0;
    while (true) {
      ++count;
      this.arangods.forEach(arangod => {
        if (arangod.pid === null) {
          // not yet launched.
          return;
        }
        if (arangod.suspended || arangod.upAndRunning) {
          // just fake the availability here
          arangod.upAndRunning = true;
          return;
        }
        if (!this.options.noStartStopLogs) {
          print(Date() + " tickeling cluster node " + arangod.url + " - " + arangod.name);
        }
        let url = arangod.url;
        if (arangod.isRole(instanceRole.coordinator) && arangod.args["javascript.enabled"] !== "false") {
          url += '/_api/foxx';
          httpOptions.method = 'GET';
        } else {
          url += '/_api/version';
          httpOptions.method = 'POST';
        }
        const reply = download(url, '', httpOptions);
        if (!this.options.noStartStopLogs) {
          print(`Server reply to ${url}: ${JSON.stringify(reply)}`);
        }
        if (!reply.error && reply.code === 200) {
          arangod.pingUntilReady(arangod.authHeaders, time() + seconds(60));
          arangod.upAndRunning = true;
          return true;
        }
        try {
          if (reply.code === 403) {
            let parsedBody = JSON.parse(reply.body);
            if (parsedBody.errorNum === errors.ERROR_SERVICE_API_DISABLED.code) {
              if (!this.options.noStartStopLogs) {
                print("service API disabled, continuing.");
              }
              arangod.upAndRunning = true;
              return true;
            }
          }
        } catch (e) {
          print(RED + Date() + " failed to parse server reply: " + JSON.stringify(reply));
        }

        if (arangod.pid !== null && !arangod.checkArangoAlive()) {
          this.arangods.forEach(arangod => {
            if ((arangod.exitStatus === null) ||
                (arangod.exitStatus.status === 'RUNNING')) {
              // arangod.killWithCoreDump();
              arangod.processSanitizerReports();
              arangod.exitStatus = killExternal(arangod.pid, termSignal);
            }
            arangod.analyzeServerCrash('startup timeout; forcefully terminating ' + arangod.name + ' with pid: ' + arangod.pid);
          });

          throw new Error(`cluster startup: pid ${arangod.pid} no longer alive! bailing out!`);
        }
        wait(0.5, false);
        return true;
      });

      let upAndRunning = 0;
      this.arangods.forEach(arangod => {
        if (arangod.upAndRunning || arangod.pid === null) {
          upAndRunning += 1;
        }
      });
      if (upAndRunning === this.arangods.length) {
        break;
      }

      // Didn't startup in 10 minutes? kill it, give up.
      if (count > 1200) {
        this.arangods.forEach(arangod => {
          arangod.killWithCoreDump('startup timeout; forcefully terminating ' + arangod.name + ' with pid: ' + arangod.pid);
        });
        this.arangods.forEach((arangod) => {
          crashUtils.aggregateDebugger(arangod, this.options);
          arangod.waitForExitAfterDebugKill();
        });
        this.cleanup = false;
        throw new Error('cluster startup timed out after 10 minutes!');
      }
    }

    if (!this.options.noStartStopLogs) {
      print("Determining server IDs");
    }
    this.arangods.forEach(arangod => {
      if (arangod.suspended || arangod.pid === null) {
        return;
      }
      // agents don't support the ID call...
      if (!arangod.isAgent() &&
          !arangod.isRole(instanceRole.single)) {
        let reply;
        try {
          httpOptions.method = 'GET';
          reply = download(arangod.url + '/_db/_system/_admin/server/id', '', httpOptions);
        } catch (e) {
          print(RED + Date() + " error requesting server '" + JSON.stringify(arangod.getStructure()) + "' Error: " + JSON.stringify(e) + RESET);
          if (e instanceof ArangoError && e.message.search('Connection reset by peer') >= 0) {
            httpOptions.method = 'GET';
            sleep(5);
            reply = download(arangod.url + '/_db/_system/_admin/server/id', '', httpOptions);
          } else {
            throw e;
          }
        }
        if (reply.error || reply.code !== 200) {
          throw new Error("Server has no detectable ID! " +
                          JSON.stringify(reply) + "\n" +
                          JSON.stringify(arangod.getStructure()));
        }
        let res = JSON.parse(reply.body);
        arangod.id = res['id'];
      }
    });
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief checks whether any instance has failure points set
  // //////////////////////////////////////////////////////////////////////////////

  checkServerFailurePoints() {
    this.rememberConnection();
    let failurePoints = [];
    this.arangods.forEach(arangod => {
      // we don't have JWT success atm, so if, skip:
      if ((!arangod.isAgent()) &&
          !arangod.args.hasOwnProperty('server.jwt-secret-folder') &&
          !arangod.args.hasOwnProperty('server.jwt-secret')) {
        let fp = arangod.debugGetFailurePoints();
        if (fp.length > 0) {
          failurePoints.push({
            "role": arangod.instanceRole,
            "pid":  arangod.pid,
            "database.directory": arangod['database.directory'],
            "failurePoints": fp
          });
        }
      }
    });
    this.reconnectMe();
    return failurePoints;
  }

  reconnect()
  {
    if (this.JWT !== null) {
      let deadline = time() + seconds(60);
      arango.reconnect(this.endpoint,
                       '_system',
                       `${this.options.username}`,
                       `${this.options.password}`,
                       time() < deadline,
                       this.JWT);
      return true;
    }
    if (this.options.hasOwnProperty('server')) {
      arango.reconnect(this.endpoint, '_system', 'root', '');
      return true;
    }

    try {
      if (this.endpoint !== null) {
        let passvoid = '';
        if (this.arangods[0].args.hasOwnProperty('database.password')) {
          passvoid = this.arangods[0].args['database.password'];
        }
        arango.reconnect(this.endpoint, '_system', 'root', passvoid);
      } else {
        print("Don't have a frontend instance to connect to");
      }
    } catch (e) {
      print(RED + Date() + " error connecting '" + this.endpoint + "' Error: " + JSON.stringify(e) + RESET);
      if (e instanceof ArangoError && e.message.search('Connection reset by peer') >= 0) {
        sleep(5);
        arango.reconnect(this.endpoint, '_system', 'root', '');
      } else if (e instanceof ArangoError && e.message.search('service unavailable due to startup or maintenance mode') >= 0) {
        sleep(5);
        arango.reconnect(this.endpoint, '_system', 'root', '');
      } else {
        throw e;
      }
    }
    return true;
  }
  /// make arangosh use HTTP, HTTP2 according to option
  findEndpoint() {
    let endpoint = this.endpoint;
    if (this.options.http2) {
      if (this.options.protocol === 'ssl') {
        endpoint = endpoint.replace(/.*\/\//, 'h2+ssl://');
      } else {
        endpoint = endpoint.replace(/.*\/\//, 'h2://');
      }
    }
    print("using endpoint ", endpoint);
    return endpoint;
  }

  setEndpoints() {
    if (this.options.cluster) {
      this.arangods.forEach(arangod => {
        if (arangod.isRole(instanceRole.coordinator)) {
          this.urls.push(arangod.url);
          this.endpoints.push(arangod.endpoint);
        }
      });
      if (this.endpoints.length === 0) {
        throw new Error("Unable to find coordinator!");
      }
      this.url = this.urls[0];
      this.endpoint = this.endpoints[0];
    } else if (this.options.agency) {
      this.arangods.forEach(arangod => {
        this.urls.push(arangod.url);
        this.endpoints.push(arangod.endpoint);
      });
      this.url = this.urls[0];
      this.endpoint = this.endpoints[0];
    } else {
      this.endpoints = [this.endpoint];
      this.urls = [this.url];
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////                Utility functionality                             ////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////
  printProcessInfo(startTime) {
    if (this.options.noStartStopLogs) {
      return;
    }
    print(CYAN + Date() + ' up and running in ' + (time() - startTime) + ' seconds' + RESET);
    var ports = [];
    var processInfo = [];
    this.arangods.forEach(arangod => {
      processInfo.push(arangod.getProcessInfo(ports));
    });
    print(processInfo.join('\n') + '\n');
  }
  printStillRunningProcessInfo() {
    if (this.options.noStartStopLogs) {
      return;
    }
    let roles = {};
    // TODO
    this.arangods.forEach(arangod => {
      if (!roles.hasOwnProperty(arangod.instanceRole)) {
        roles[arangod.instanceRole] = 0;
      }
      ++roles[arangod.instanceRole];
    });
    let roleNames = [];
    for (let r in roles) {
      // e.g. 2 + coordinator + (s)
      roleNames.push(roles[r] + ' ' + r + '(s)');
    }
    print(roleNames.join(', ') + ' are still running...');
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief manage package capture of this instance
  // //////////////////////////////////////////////////////////////////////////////

  launchTcpDump(name) {
    if (this.options.sniff === undefined || this.options.sniff === false) {
      return true;
    }
    this.options.cleanup = false;
    let device = 'lo';
    if (this.options.sniffDevice !== undefined) {
      device = this.options.sniffDevice;
    }

    let prog = 'tcpdump';
    if (this.options.sniffProgram !== undefined) {
      prog = this.options.sniffProgram;
    }

    let pcapFile = fs.join(this.rootDir, name + 'out.pcap');
    let args;
    if (prog === 'ngrep') {
      args = ['-l', '-Wbyline', '-d', device];
    } else {
      args = ['-ni', device, '-s0', '-w', pcapFile];
    }
    let count = 0;
    this.arangods.forEach(arangod => {
      if (!this.options.sniffAgency && arangod.isAgent()){
        return;
      }
      if (!this.options.sniffDBServers && arangod.isRole(instanceRole.dbServer)) {
        return;
      }
      if (count > 0) {
        args.push('or');
      }
      args.push('port');
      args.push(arangod.port);
      count ++;
    });

    if (this.options.sniff === 'sudo') {
      args.unshift(prog);
      prog = 'sudo';
    }
    print(CYAN + 'launching ' + prog + ' ' + JSON.stringify(args) + RESET);
    try {
      this.tcpdump = executeExternal(prog, args);
      sleep(5);
      let exitStatus = statusExternal(this.tcpdump.pid, false);
      if (exitStatus.status !== "RUNNING") {
        crashUtils.GDB_OUTPUT += `Failed to launch tcpdump: ${JSON.stringify(exitStatus)} '${prog}' ${JSON.stringify(args)}`;
        this.tcpdump = null;
        return false;
      }
    } catch (x) {
      crashUtils.GDB_OUTPUT += `Failed to launch tcpdump: ${x.message} ${prog} ${JSON.stringify(args)}`;
      return false;
    }
    return true;
  }
  stopTcpDump() {
    if (this.tcpdump !== null) {
      print(CYAN + "Stopping tcpdump" + RESET);
      killExternal(this.tcpdump.pid);
      try {
        statusExternal(this.tcpdump.pid, true);
      } catch (x)
      {
        print(Date() + ' wasn\'t able to stop tcpdump: ' + x.message );
      }
      this.tcpdump = null;
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief get process stats over the SUT
  // //////////////////////////////////////////////////////////////////////////////
  initProcessStats() {
    this.arangods.forEach((arangod) => { arangod.getProcessStats();  });
  }

  getDeltaProcessStats(instanceInfo) {
    try {
      let deltaStats = {};
      let deltaSum = {};
      this.arangods.forEach((arangod) => {
        let oneDeltaStats = arangod.getDeltaProcessStats();
        for (let key in oneDeltaStats) {
          if (deltaSum.hasOwnProperty(key)) {
            deltaSum[key] += oneDeltaStats[key];
          } else {
            deltaSum[key] = oneDeltaStats[key];
          }
        }
      });
      deltaStats['sum_servers'] = deltaSum;
      return deltaStats;
    }
    catch (x) {
      print("aborting stats generation: " + x);
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
  // / @brief memory profiling infrastructure
  // //////////////////////////////////////////////////////////////////////////////

  getMemProfSnapshot(instanceInfo, options, counter) {
    if (this.options.memprof) {
      let opts = Object.assign(pu.makeAuthorizationHeaders(this.options, this.addArgs),
                               { method: 'GET' });

      this.arangods.forEach(arangod => arangod.getMemprofSnapshot(opts));
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief check how many sockets the SUT uses
  // //////////////////////////////////////////////////////////////////////////////

  getNetstat() {
    let ret = {};
    this.arangods.forEach( arangod => {
      ret[arangod.name] = arangod.netstat;;
    });
    return ret;
  }

  printNetstat() {
    this.arangods.forEach( arangod => {
      print(arangod.name + " => " + JSON.stringify(arangod.netstat));
    });
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief record the resource usage of the SUT
  // //////////////////////////////////////////////////////////////////////////////
  spawnClusterHealthMonitor() {
    if ((this.options.cluster || this.options.agency) &&
        !this.hasOwnProperty('clusterHealthMonitor') &&
        !this.options.disableClusterMonitor) {
      print("spawning cluster health inspector");
      internal.env.INSTANCEINFO = JSON.stringify(this.getStructure());
      internal.env.OPTIONS = JSON.stringify(this.options);
      let tmp = internal.env.TEMP;
      internal.env.TMP = this.rootDir;
      internal.env.TEMP = this.rootDir;
      let args = pu.makeArgs.arangosh(this.options);
      args['javascript.allow-external-process-control'] =  true;
      args['javascript.execute'] = fs.join('js', 'client', 'modules', '@arangodb', 'testutils', 'clusterstats.js');
      const argv = toArgv(args);
      this.clusterHealthMonitor = executeExternal(pu.ARANGOSH_BIN, argv);
      this.clusterHealthMonitorFile = fs.join(this.rootDir, 'stats.jsonl');
      internal.env.TMP = tmp;
      internal.env.TEMP = tmp;
    }
    if (!this.options.disableClusterMonitor) {
      this.initProcessStats();
    }
  }
  stopClusterHealthMonitor() {
    if (this.hasOwnProperty('clusterHealthMonitor')) {
      try {
        this.clusterHealthMonitor['kill'] = killExternal(this.clusterHealthMonitor.pid);
        this.clusterHealthMonitor['statusExternal'] = statusExternal(this.clusterHealthMonitor.pid, true);
      }
      catch (x) {
        print(x);
      }
    }
  }

}

exports.instanceManager = instanceManager;
exports.registerOptions = function(optionsDefaults, optionsDocumentation, optionHandlers) {
  tu.CopyIntoObject(optionsDefaults, {
    'memory': undefined,
    'singles': 1, // internal only
    'coordinators': 1,
    'dbServers': 2,
    'disableClusterMonitor': true,
    'encryptionAtRest': false,
    'extraArgs': {},
    'cluster': false,
    'forceOneShard': false,
    'sniff': false,
    'sniffAgency': true,
    'sniffDBServers': true,
    'sniffDevice': undefined,
    'sniffProgram': undefined,
    'sniffFilter': undefined,
    'sleepBeforeStart' : 0,
    'memprof': false,
    'valgrind': false,
    'valgrindFileBase': '',
    'valgrindArgs': {},
    'valgrindHosts': false,
  });

  tu.CopyIntoList(optionsDocumentation, [
    ' SUT configuration properties:',
    '   - `memory`: amount of Host memory to distribute amongst the arangods',
    '   - `encryptionAtRest`: enable on disk encryption, enterprise only',
    '   - `cluster`: if set to true the tests are run with a cluster',
    '   - `forceOneShard`: if set to true the tests are run with a OneShard (EE only) cluster, requires cluster option to be set to true',
    '   - `dbServers`: number of DB-Servers to use',
    '   - `coordinators`: number coordinators to use',
    '   - `extraArgs`: list of extra commandline arguments to add to arangod',
    ' SUT monitoring',
    '   - `sleepBeforeStart` : sleep at tcpdump info - use this to dump traffic or attach debugger',
    '   - `disableClusterMonitor`: if set to false, an arangosh is started that will send',
    '                              keepalive requests to all cluster instances, and report on error',
    ' SUT debugging',
    '   - `server`: server_url (e.g. tcp://127.0.0.1:8529) for external server',
    '   - `serverRoot`: directory where data/ points into the db server. Use in',
    '                   conjunction with "server".',
    '   - `memprof`: take snapshots (requries memprof enabled build)',
    ' SUT packet capturing:',
    '   - `sniff`: if we should try to launch tcpdump / windump for a testrun',
    '              false / true / sudo',
    '   - `sniffDevice`: the device tcpdump / tshark should use',
    '   - `sniffProgram`: specify your own programm',
    '   - `sniffAgency`: when sniffing cluster, sniff agency traffic too? (true)',
    '   - `sniffDBServers`: when sniffing cluster, sniff dbserver traffic too? (true)',
    '   - `sniffFilter`: only launch tcpdump for tests matching this string',
    ' SUT & valgrind:',
    '   - `valgrind`: if set the programs are run with the valgrind',
    '     memory checker; should point to the valgrind executable',
    '   - `valgrindFileBase`: string to prepend to the report filename',
    '   - `valgrindArgs`: commandline parameters to add to valgrind',
    '   - valgrindHosts  - configure which clustercomponents to run using valgrind',
    '        Coordinator - flag to run Coordinator with valgrind',
    '        DBServer    - flag to run DBServers with valgrind',
    ''
  ]);
  optionHandlers.push(function(options) {
    if (options.forceOneShard) {
      if (!options.cluster) {
        throw new Error("need cluster enabled");
      }
    }
    if (options.memprof) {
      process.env['MALLOC_CONF'] = 'prof:true';
    }
    options.noStartStopLogs = !options.extremeVerbosity && options.noStartStopLogs;
    if (options.encryptionAtRest && !pu.isEnterpriseClient) {
      options.encryptionAtRest = false;
    }
  });
};
