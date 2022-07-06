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
const inst = require('@arangodb/testutils/instance');
const yaml = require('js-yaml');
const internal = require('internal');
const crashUtils = require('@arangodb/testutils/crash-utils');
const crypto = require('@arangodb/crypto');
const ArangoError = require('@arangodb').ArangoError;
const debugGetFailurePoints = require('@arangodb/test-helper').debugGetFailurePoints;
const netstat = require('node-netstat');
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
const IS_A_TTY = internal.isATTy();

const platform = internal.platform;

const termSignal = 15;

const instanceRole = inst.instanceRole;

class instanceManager {
  constructor(protocol, options, addArgs, testname, tmpDir) {
    this.protocol = protocol;
    this.options = options;
    this.addArgs = addArgs;
    this.rootDir = fs.join(tmpDir || fs.getTempPath(), testname);
    this.options.agency = this.options.agency || this.options.cluster || this.options.activefailover;
    this.agencyConfig = new inst.agencyConfig(options, this);
    this.leader = null;
    this.urls = [];
    this.endpoints = [];
    this.arangods = [];
    this.restKeyFile = '';
    this.tcpdump = null;
    this.JWT = null;
    this.cleanup = options.cleanup && options.server === undefined;
    if (addArgs.hasOwnProperty('server.jwt-secret')) {
      this.JWT = addArgs['server.jwt-secret'];
    }
    if (this.options.encryptionAtRest) {
      this.restKeyFile = fs.join(this.rootDir, 'openSesame.txt');
      fs.makeDirectoryRecursive(this.rootDir);
      fs.write(this.restKeyFile, "Open Sesame!Open Sesame!Open Ses");
    }
    this.httpAuthOptions = pu.makeAuthorizationHeaders(this.options, addArgs);
  }

  destructor(cleanup) {
    this.arangods.forEach(arangod => {
      arangod.pm.deregister(arangod.port);
    });
    this.stopTcpDump();
    if (this.cleanup && cleanup) {
      this._cleanup();
    }
  }
  getStructure() {
    let d = [];
    let ln = "";
    if (this.leader !== null) {
      ln = this.leader.name;
    }
    this.arangods.forEach(arangod => { d.push(arangod.getStructure());});
    return {
      protocol: this.protocol,
      options: this.options,
      addArgs: this.addArgs,
      rootDir: this.rootDir,
      leader: ln,
      agencyConfig: this.agencyConfig.getStructure(),
      httpAuthOptions: this.httpAuthOptions,
      urls: this.urls,
      url: this.url,
      endpoints: this.endpoints,
      endpoint: this.endpoint,
      arangods: d,
      restKeyFile: this.restKeyFile,
      tcpdump: this.tcpdump,
      cleanup: this.cleanup
    };
  }
  setFromStructure(struct) {
    this.arangods = [];
    this.agencyConfig.setFromStructure(struct['agencyConfig']);
    this.protocol = struct['protocol'];
    this.options = struct['options'];
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
      let oneArangod = new inst.instance(this.options, '', {}, {}, '', '', '', this.agencyConfig);
      this.arangods.push(oneArangod);
      oneArangod.setFromStructure(arangodStruct);
      if (oneArangod.isAgent()) {
        this.agencyConfig.agencyInstances.push(oneArangod);
      }
    });
    let ln = struct['leader'];
    if (ln !== "") {
      this.arangods.forEach(arangod => {
        if (arangod.name === ln) {
          this.leader = arangod;
        }
      });
    }
  }

  _cleanup() {
    this.arangods.forEach(instance => { instance.cleanup(); });
    if (this.options.extremeVerbosity) {
      print(CYAN + "cleaning up manager's Directory: " + this.rootDir + RESET);
    }
    fs.removeDirectoryRecursive(this.rootDir, true);
  }
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief prepares all instances required for a deployment
  // /
  // / protocol must be one of ["tcp", "ssl", "unix"]
  // //////////////////////////////////////////////////////////////////////////////

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

      for (let count = 0;
           this.options.agency && count < this.agencyConfig.agencySize;
           count ++) {
        this.arangods.push(new inst.instance(this.options,
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
        this.arangods.push(new inst.instance(this.options,
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
        this.arangods.push(new inst.instance(this.options,
                                             instanceRole.coordinator,
                                             this.addArgs,
                                             this.httpAuthOptions,
                                             this.protocol,
                                             fs.join(this.rootDir, instanceRole.coordinator + "_" + count),
                                             this.restKeyFile,
                                             this.agencyConfig));
        frontendCount ++;
      }
      
      for (let count = 0;
           this.options.activefailover && count < this.options.singles;
           count ++) {
        this.arangods.push(new inst.instance(this.options,
                                             instanceRole.failover,
                                             this.addArgs,
                                             this.httpAuthOptions,
                                             this.protocol,
                                             fs.join(this.rootDir, instanceRole.failover + "_" + count),
                                             this.restKeyFile,
                                             this.agencyConfig));
        this.urls.push(this.arangods[this.arangods.length -1].url);
        this.endpoints.push(this.arangods[this.arangods.length -1].endpoint);
        frontendCount ++;
      }

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
                                             this.agencyConfig));
        this.urls.push(this.arangods[this.arangods.length -1].url);
        this.endpoints.push(this.arangods[this.arangods.length -1].endpoint);
        frontendCount ++;
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
    const startTime = time();
    try {
      this.arangods.forEach(arangod => arangod.startArango());
      if (this.options.cluster) {
        this.checkClusterAlive();
      } else if (this.options.activefailover) {
        if (this.urls !== []) {
          this.detectCurrentLeader();
        }
      }
      this.launchFinalize(startTime);
      return true;
    } catch (e) {
      print(e, e.stack);
      return false;
    }
  }

  restartOneInstance(moreArgs, startTime) {
    /// todo
    if (this.options.cluster && !this.options.skipReconnect) {
      this.checkClusterAlive({}); // todo addArgs
      if (this.JWT) {
        print("reconnecting with JWT " + this.endpoint);
        arango.reconnect(this.endpoint,
                         '_system',
                         this.options.username,
                         this.options.password,
                         false,
                         this.JWT);
      } else {
        print("reconnecting " + this.endpoint);
        arango.reconnect(this.endpoint,
                         '_system',
                         this.options.username,
                         this.options.password,
                         false);
      }
    }
    this.launchFinalize(startTime);
  }
  printProcessInfo(startTime) {
    if (this.options.noStartStopLogs) {
      return;
    }
    print(CYAN + Date() + ' up and running in ' + (time() - startTime) + ' seconds' + RESET);
    var matchPort = /.*:.*:([0-9]*)/;
    var ports = [];
    var processInfo = [];
    this.arangods.forEach(arangod => {
      let res = matchPort.exec(arangod.endpoint);
      if (!res) {
        return;
      }
      var port = res[1];
      if (arangod.isAgent()) {
        if (this.options.sniffAgency) {
          ports.push('port ' + port);
        }
      } else if (arangod.isRole(instanceRole.dbServer)) {
        if (this.options.sniffDBServers) {
          ports.push('port ' + port);
        }
      } else {
        ports.push('port ' + port);
      }
      processInfo.push('  [' + arangod.name +
                       '] up with pid ' + arangod.pid +
                       ' - ' + arangod.dataDir);
    });
    print(processInfo.join('\n') + '\n');
  }
  launchTcpDump(name) {
    if (this.options.sniff === undefined || this.options.sniff === false) {
      return;
    }
    this.options.cleanup = false;
    let device = 'lo';
    if (platform.substr(0, 3) === 'win') {
      device = '1';
    }
    if (this.options.sniffDevice !== undefined) {
      device = this.options.sniffDevice;
    }

    let prog = 'tcpdump';
    if (platform.substr(0, 3) === 'win') {
      prog = 'c:/Program Files/Wireshark/tshark.exe';
    }
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
    this.tcpdump = executeExternal(prog, args);
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
  // / @brief scans the log files for important infos
  // //////////////////////////////////////////////////////////////////////////////

  readImportantLogLines (logPath) {
    let importantLines = {};
    // TODO: iterate over instances
    // for (let i = 0; i < list.length; i++) {
    // }

    return importantLines;
  }

  initProcessStats() {
    this.arangods.forEach((arangod) => {
      arangod.stats = arangod._getProcessStats();
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
    if (this.options.memprof) {
      let opts = Object.assign(pu.makeAuthorizationHeaders(this.options, this.addArgs),
                               { method: 'GET' });

      this.arangods.forEach(arangod => arangod.getMemprofSnapshot(opts));
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
  dumpAgency() {
    this.arangods.forEach((arangod) => {
      if (arangod.role === "agent") {
        if (arangod.hasOwnProperty('exitStatus')) {
          print(Date() + " this agent is already dead: " + JSON.stringify(arangod.getStructure()));
        } else {
          print(Date() + " Attempting to dump Agent: " + JSON.stringify(arangod.getStructure()));
          arangod.dumpAgent( '/_api/agency/config', 'GET', 'agencyConfig');

          arangod.dumpAgent('/_api/agency/state', 'GET', 'agencyState');

          arangod.dumpAgent('/_api/agency/read', 'POST', 'agencyPlan');
        }
      }
    });
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
      const health = internal.clusterHealth();
      return this.arangods.every((arangod) => {
        name = "on node " + arangod.name;
        if (arangod.isAgent() || arangod.isRole(instanceRole.single)) {
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
      print("Error checking cluster health " + name + " => " + e);
      return false;
    }
    return true;
  }

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

  // skipHealthCheck can be set to true to avoid a call to /_admin/cluster/health
  // on the coordinator. This is necessary if only the agency is running yet.
  checkInstanceAlive({skipHealthCheck = false} = {}) {
    this.arangods.forEach(arangod => { arangod.netstat = {'in':{}, 'out': {}};});
    let obj = this;
    netstat({platform: process.platform}, function (data) {
      // skip server ports, we know what we bound.
      if (data.state !== 'LISTEN') {
        obj.arangods.forEach(arangod => arangod.checkNetstat(data));
      }
    });
    if (!this.options.noStartStopLogs) {
      this.printNetstat();
    }
    
    if (this.options.activefailover &&
        this.hasOwnProperty('authOpts') &&
        (this.url !== this.agencyConfig.urls[0])
       ) {
      // only detect a leader after we actually know one has been started.
      // the agency won't tell us anything about leaders.
      let d = this.detectCurrentLeader();
      if (this.endpoint !== d.endpoint) {
        print(Date() + ' failover has happened, leader is no more! Marking Crashy!');
        d.serverCrashedLocal = true;
        this.dumpAgency();
        return false;
      }
    }

    let rc = this.arangods.reduce((previous, arangod) => {
      let ret = arangod.checkArangoAlive();
      if (!ret) {
        this.message += arangod.message;
      }
      return previous && ret;
    }, true);
    if (rc && this.options.cluster && !skipHealthCheck && this.arangods.length > 1) {
      const seconds = x => x * 1000;
      const checkAllAlive = () => this.arangods.every(arangod => arangod.checkArangoAlive());
      let first = true;
      rc = false;
      for (
        const start = Date.now();
        !rc && Date.now() < start + seconds(60) && checkAllAlive();
        internal.sleep(1)
      ) {
        rc = this._checkServersGOOD();
        if (first) {
          if (!this.options.noStartStopLogs) {
            print(RESET + "Waiting for all servers to go GOOD...");
          }
          first = false;
        }
      }
    }
    if (!rc) {
      this.dumpAgency();
      print(Date() + ' If cluster - will now start killing the rest.');
      this.arangods.forEach((arangod) => {
        print(Date() + " Killing in the name of: ");
        arangod.killWithCoreDump();
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

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief shuts down an instance
  // //////////////////////////////////////////////////////////////////////////////

  shutdownInstance (forceTerminate) {
    if (forceTerminate === undefined) {
      forceTerminate = false;
    }
    let shutdownSuccess = !forceTerminate;

    // we need to find the leading server
    if (this.options.activefailover) {
      let d = this.detectCurrentLeader();
      if (this.endpoint !== d.endpoint) {
        print(Date() + ' failover has happened, leader is no more! Marking Crashy!');
        d.serverCrashedLocal = true;
        forceTerminate = true;
        shutdownSuccess = false;
      }
    }

    if (!this.checkInstanceAlive()) {
      print(Date() + ' Server already dead, doing nothing. This shouldn\'t happen?');
    }

    if (!forceTerminate) {
      try {
        // send a maintenance request to any of the coordinators, so that
        // no failed server/failed follower jobs will be started on shutdown
        let coords = this.arangods.filter(arangod =>
          arangod.isRole(instanceRole.coordinator) &&
            (arangod.exitStatus !== null));
        if (coords.length > 0) {
          let requestOptions = pu.makeAuthorizationHeaders(this.options, this.addArgs);
          requestOptions.method = 'PUT';

          if (!this.options.noStartStopLogs) {
            print(coords[0].url + "/_admin/cluster/maintenance");
          }
          download(coords[0].url + "/_admin/cluster/maintenance", JSON.stringify("on"), requestOptions);
        }
      } catch (err) {
        print(Date() + " error while setting cluster maintenance mode:", err);
        shutdownSuccess = false;
      }
    }

    if (this.options.cluster && this.hasOwnProperty('clusterHealthMonitor')) {
      try {
        this.clusterHealthMonitor['kill'] = killExternal(this.clusterHealthMonitor.pid);
        this.clusterHealthMonitor['statusExternal'] = statusExternal(this.clusterHealthMonitor.pid, true);
      }
      catch (x) {
        print(x);
      }
    }

    // Shut down all non-agency servers:
    const n = this.arangods.length;

    let toShutdown = this.arangods.slice();
    toShutdown.sort((a, b) => {
      if (a.instanceRole === b.instanceRole) return 0;
      if (a.isRole(instanceRole.coordinator) &&
          b.isRole(instanceRole.dbServer)) return -1;
      if (b.isRole(instanceRole.coordinator) &&
          a.isRole(instanceRole.dbServer)) return 1;
      if (a.isAgent()) return 1;
      if (b.isAgent()) return -1;
      return 0;
    });
    if (!this.options.noStartStopLogs) {
      let shutdown = [];
      toShutdown.forEach(arangod => {shutdown.push(arangod.name);});
      print(Date() + ' Shutdown order: \n' + yaml.safeDump(shutdown));
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

    if ((toShutdown.length > 0) && (this.options.agency === true) && (this.options.dumpAgencyOnError === true)) {
      this.dumpAgency();
    }
    var shutdownTime = internal.time();
    while (toShutdown.length > 0) {
      toShutdown = toShutdown.filter(arangod => {
        if (arangod.exitStatus === null) {
          if ((nonAgenciesCount > 0) && arangod.isAgent()) {
            return true;
          }
          arangod.shutdownArangod(forceTerminate);
          if (forceTerminate) {
            print(Date() + " FORCED shut down: " + JSON.stringify(arangod.getStructure()));
          } else {
            arangod.exitStatus = {
              status: 'RUNNING'
            };

            if (!this.options.noStartStopLogs) {
              print(Date() + " Commanded shut down: " + arangod.name);
            }
          }
          return true;
        }
        if ((arangod.exitStatus !== null) && (arangod.exitStatus.status === 'RUNNING')) {
          arangod.exitStatus = statusExternal(arangod.pid, false);
          if (!crashUtils.checkMonitorAlive(pu.ARANGOD_BIN, arangod, this.options, arangod.exitStatus)) {
            if (!arangod.isAgent()) {
              nonAgenciesCount--;
            }
            print(Date() + ' Server "' + arangod.name + '" shutdown: detected irregular death by monitor: pid', arangod.pid);
            return false;
          }
        }
        if ((arangod.exitStatus !== null) && (arangod.exitStatus.status === 'RUNNING')) {
          let localTimeout = timeout;
          if (arangod.isAgent()) {
            localTimeout = localTimeout + 60;
          }
          if ((internal.time() - shutdownTime) > localTimeout) {
            this.dumpAgency();
            print(Date() + ' forcefully terminating ' + yaml.safeDump(arangod.getStructure()) +
                  ' after ' + timeout + 's grace period; marking crashy.');
            arangod.serverCrashedLocal = true;
            shutdownSuccess = false;
            arangod.killWithCoreDump();
            arangod.analyzeServerCrash('shutdown timeout; instance "' +
                                       arangod.name +
                                       '" forcefully KILLED after 60s - ' +
                                       arangod.exitStatus.signal);
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
          crashUtils.stopProcdump(this.options, arangod);
        } else {
          if (!arangod.isAgent()) {
            nonAgenciesCount--;
          }
          if (!this.options.noStartStopLogs) {
            print(Date() + ' Server "' + arangod.name + '" shutdown: Success: pid', arangod.pid);
          }
          crashUtils.stopProcdump(this.options, arangod);
          return false;
        }
      });
      if (toShutdown.length > 0) {
        let roles = {};
        // TODO
        toShutdown.forEach(arangod => {
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
        if (!this.options.noStartStopLogs) {
          print(roleNames.join(', ') + ' are still running...');
        }
        require('internal').wait(1, false);
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
      arangod.readAssertLogLines();
    });
    this.cleanup = shutdownSuccess;
    return shutdownSuccess;
  }

  detectCurrentLeader(instanceInfo) {
    let opts = {
      method: 'POST',
      jwt: crypto.jwtEncode(this.arangods[0].args['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256'),
      headers: {'content-type': 'application/json' }
    };
    let count = 10;
    while (count > 0) {
      let reply = download(this.agencyConfig.urls[0] + '/_api/agency/read', '[["/arango/Plan/AsyncReplication/Leader"]]', opts);

      if (!reply.error && reply.code === 200) {
        let res = JSON.parse(reply.body);
        print("Response ====> " + reply.body);
        if (res[0].hasOwnProperty('arango') && res[0].arango.Plan.AsyncReplication.hasOwnProperty('Leader')){
          let leader = res[0].arango.Plan.AsyncReplication.Leader;
          if (leader) {
            break;
          }
        }
        count --;
        if (count === 0) {
          throw "Leader is not selected";
        }
        sleep(0.5);
      }
    }
    this.leader = null;
    while (this.leader === null) {
      this.urls.forEach(url => {
        opts['method'] = 'GET';
        let reply = download(url + '/_api/cluster/endpoints', '', opts);
        if (reply.code === 200) {
          let res;
          try {
            res = JSON.parse(reply.body);
          }
          catch (x) {
            throw "Failed to parse endpoints reply: " + JSON.stringify(reply);
          }
          let leaderEndpoint = res.endpoints[0].endpoint;
          let leaderInstance;
          this.arangods.forEach(d => {
            if (d.endpoint === leaderEndpoint) {
              leaderInstance = d;
            }
          });
          this.leader = leaderInstance;
          this.url = leaderInstance.url;
          this.endpoint = leaderInstance.endpoint;
        }
        if (this.options.extremeVerbosity) {
          print(url + " not a leader " + JSON.stringify(reply));
        }
      });
      sleep(0.5);
    }
    if (this.options.extremeVerbosity) {
      print("detected leader: " + this.leader.name);
    }
    return this.leader;
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
          url += '/_admin/aardvark/index.html';
          httpOptions.method = 'GET';
        } else {
          url += '/_api/version';
          httpOptions.method = 'POST';
        }
        const reply = download(url, '', httpOptions);
        if (!reply.error && reply.code === 200) {
          arangod.upAndRunning = true;
          return true;
        }

        if (arangod.pid !== null && !arangod.checkArangoAlive()) {
          this.arangods.forEach(arangod => {
            if ((arangod.exitStatus === null) ||
                (arangod.exitStatus.status === 'RUNNING')) {
              // arangod.killWithCoreDump();
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
          arangod.killWithCoreDump();
          arangod.analyzeServerCrash('startup timeout; forcefully terminating ' + arangod.name + ' with pid: ' + arangod.pid);
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
            internal.sleep(5);
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
  reconnect()
  {
    // we need to find the leading server
    if (this.options.activefailover) {
      internal.wait(5.0, false);
      let d = this.detectCurrentLeader();
      if (d === undefined) {
        throw "failed to detect a leader";
      }
      this.endpoint = d.endpoint;
      this.url = d.url;
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
        internal.sleep(5);
        arango.reconnect(this.endpoint, '_system', 'root', '');
      } else {
        throw e;
      }
    }
    return true;
  }
  /// make arangosh use HTTP, VST or HTTP2 according to option
  findEndpoint() {
    let endpoint = this.endpoint;
    if (this.options.vst) {
      endpoint = endpoint.replace(/.*\/\//, 'vst://');
    } else if (this.options.http2) {
      endpoint = endpoint.replace(/.*\/\//, 'h2://');
    }
    print("using endpoint ", endpoint);
    return endpoint;
  }

  launchFinalize(startTime) {
    if (!this.options.cluster && !this.options.agency) {
      let httpOptions = _.clone(this.httpAuthOptions);
      httpOptions.method = 'POST';
      httpOptions.returnBodyOnError = true;
      let count = 0;
      this.arangods.forEach(arangod => {
        if (arangod.suspended) {
          return;
        }
        while (true) {
          wait(1, false);
          if (this.options.useReconnect && arangod.isFrontend()) {
            try {
              if (this.JWT) {
                print(Date() + " reconnecting with JWT " + arangod.url);
                arango.reconnect(arangod.endpoint,
                                 '_system',
                                 this.options.username,
                                 this.options.password,
                                 count > 50,
                                 this.JWT);
              } else {
                print(Date() + " reconnecting " + arangod.url);
                arango.reconnect(arangod.endpoint,
                                 '_system',
                                 this.options.username,
                                 this.options.password,
                                 count > 50);
              }
              break;
            } catch (e) {
              this.arangods.forEach( arangod => {
                let status = statusExternal(arangod.pid, false);
                if (status.status !== "RUNNING") {
                  throw new Error(`Arangod ${arangod.pid} exited instantly! ` + JSON.stringify(status));
                }
              });
              print(Date() + " caught exception: " + e.message);
            }
          } else {
            print(Date() + " tickeling " + arangod.url);
            const reply = download(arangod.url + '/_api/version', '', httpOptions);

            if (!reply.error && reply.code === 200) {
              break;
            }
          }
          ++count;

          if (count % 60 === 0) {
            if (!arangod.checkArangoAlive()) {
              throw new Error('startup failed! bailing out!');
            }
          }
          if (count === 300) {
            throw new Error('startup timed out! bailing out!');
          }
        }
      });
      this.endpoints = [this.endpoint];
      this.urls = [this.url];
    } else if (this.options.agency && !this.options.cluster && !this.options.activefailover) {
      this.arangods.forEach(arangod => {
        this.urls.push(arangod.url);
        this.endpoints.push(arangod.endpoint);
      });
      this.url = this.urls[0];
      this.endpoint = this.endpoints[0];
    } else {
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
    }
    
    this.printProcessInfo(startTime);
    internal.sleep(this.options.sleepBeforeStart);
    if ((this.options.cluster || this.options.agency) &&
        !this.hasOwnProperty('clusterHealthMonitor') &&
        !this.options.disableClusterMonitor) {
      print("spawning cluster health inspector");
      internal.env.INSTANCEINFO = JSON.stringify(this.getStructure());
      internal.env.OPTIONS = JSON.stringify(this.options);
      let args = pu.makeArgs.arangosh(this.options);
      args['javascript.execute'] = fs.join('js', 'client', 'modules', '@arangodb', 'testutils', 'clusterstats.js');
      const argv = toArgv(args);
      this.clusterHealthMonitor = executeExternal(pu.ARANGOSH_BIN, argv);
      this.clusterHealthMonitorFile = fs.join(this.rootDir, 'stats.jsonl');
    }
    if (!this.options.disableClusterMonitor) {
      this.initProcessStats();
    }
  }
  
  reStartInstance(moreArgs) {
    const startTime = time();
    this.addArgs = _.defaults(this.addArgs, moreArgs);
    this.httpAuthOptions = pu.makeAuthorizationHeaders(this.options, this.addArgs);
    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      oneInstance.exitStatus = null;
      oneInstance.pid = null;
      oneInstance.upAndRunning = false;
    });

    let success = true;
    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      if (oneInstance.isAgent()) {
        print("relaunching: " + oneInstance.name);
        oneInstance.launchInstance(moreArgs);
        success = success && oneInstance.checkArangoAlive();
      }
    });
    if (!success) {
      throw new Error('startup of agency failed! bailing out!');
    }
    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid !== null) {
        return;
      }
      if (oneInstance.isRole(instanceRole.dbServer)) {
        print("relaunching: " + oneInstance.name);
        oneInstance.launchInstance(moreArgs);
      }
    });
    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid !== null) {
        return;
      }
      if (oneInstance.isRole(instanceRole.coordinator)) {
        print("relaunching: " + oneInstance.name);
        oneInstance.launchInstance(moreArgs);
      }
    });
    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid !== null) {
        return;
      }
      if (oneInstance.isRole(instanceRole.single)) {
        oneInstance.launchInstance(moreArgs);
      }
    });

    if (this.options.cluster && !this.options.skipReconnect) {
      this.checkClusterAlive({}); // todo addArgs
      print("reconnecting " + this.endpoint);
      let JWT;
      if (this.addArgs && this.addArgs.hasOwnProperty('server.jwt-secret')) {
        JWT = this.addArgs['server.jwt-secret'];
      }
      if (moreArgs && moreArgs.hasOwnProperty('server.jwt-secret')) {
        JWT = moreArgs['server.jwt-secret'];
      }
      arango.reconnect(this.endpoint,
                       '_system',
                       this.options.username,
                       this.options.password,
                       false,
                       JWT);
    }
    this.launchFinalize(startTime);
  }
}

exports.instanceManager = instanceManager;
