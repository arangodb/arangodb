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
    this.httpAuthOptions = {};
    this.urls = [];
    this.endpoints = [];
    this.arangods = [];
    this.restKeyFile = '';
    if (this.options.encryptionAtRest) {
      this.restKeyFile = fs.join(this.rootDir, 'openSesame.txt');
      fs.makeDirectoryRecursive(this.rootDir);
      fs.write(this.restKeyFile, "Open Sesame!Open Sesame!Open Ses");
    }
    this.httpAuthOptions = pu.makeAuthorizationHeaders(this.options);
  }
  
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief prepares all instances required for a deployment
  // /
  // / protocol must be one of ["tcp", "ssl", "unix"]
  // //////////////////////////////////////////////////////////////////////////////

  prepareInstance () {
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
      }
      
      for (let count = 0;
           this.options.activefailover && count < this.singles;
           count ++) {
        this.arangods.push(new inst.instance(this.options,
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
        this.arangods.push(new inst.instance(this.options,
                                             instanceRole.single,
                                             this.addArgs,
                                             this.httpAuthOptions,
                                             this.protocol,
                                             fs.join(this.rootDir, instanceRole.single + "_" + count),
                                             this.restKeyFile,
                                             this.agencyConfig));
      }
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
        this.detectCurrentLeader();
      }
      this.launchFinalize(startTime);
      return true;
    } catch (e) {
      print(e, e.stack);
      return false;
    }
  }

  restartOneInstance(moreArgs) {
    /// todo
    if (this.options.cluster && !this.options.skipReconnect) {
      this.checkClusterAlive(instanceInfo, {}); // todo addArgs
      print("reconnecting " + instanceInfo.endpoint);
      arango.reconnect(this.endpoint,
                       '_system',
                       this.options.username,
                       this.options.password,
                       false
                      );
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
      if (arangod.isRole(instanceRole.agent)) {
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
    for (let port = 0; port < ports.length; port ++) {
      if (port > 0) {
        args.push('or');
      }
      args.push(ports[port]);
    }

    if (this.options.sniff === 'sudo') {
      args.unshift(prog);
      prog = 'sudo';
    }
    print(CYAN + 'launching ' + prog + ' ' + JSON.stringify(args) + RESET);
    tcpdump = executeExternal(prog, args);
  }
  stopTcpDump() {
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
    if (this.options.memprof) {
      let opts = Object.assign(pu.makeAuthorizationHeaders(this.options),
                               { method: 'GET' });

      this.arangods.forEach(arangod.getMemprofSnapshot(opts));
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

  _checkServersGOOD() {
    try {
      const health = internal.clusterHealth();
      return this.arangods.every((arangod) => {
        if (arangod.isRole(instanceRole.agent) || arangod.isRole(instanceRole.single)) {
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
  checkInstanceAlive({skipHealthCheck = false} = {}) {
    if (this.options.activefailover &&
        this.hasOwnProperty('authOpts') &&
        (this.url !== this.agencyUrl)
       ) {
      // only detect a leader after we actually know one has been started.
      // the agency won't tell us anything about leaders.
      let d = this.detectCurrentLeader();
      if (this.endpoint !== d.endpoint) {
        print(Date() + ' failover has happened, leader is no more! Marking Crashy!');
        serverCrashedLocal = true;
        this.dumpAgency();
        return false;
      }
    }

    let rc = this.arangods.reduce((previous, arangod) => {
      let ret = arangod.checkArangoAlive();
      if (!ret) {
        instanceInfo.message += arangod.message;
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
        arangod.killWithCoreDump();
        arangod.analyzeServerCrash('shutdown timeout; instance forcefully KILLED because of fatal timeout in testrun ' + sockStat);
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
        print(Date() + ' forcefully terminating ' + yaml.safeDump(arangod.getStructure()) +
              ' after ' + timeout + 's grace period; marking crashy.');
        serverCrashedLocal = true;
        counters.shutdownSuccess = false;
        arangod.killWithCoreDump();
        arangod.analyzeServerCrash('shutdown timeout; instance "' +
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
        arangod.analyzeServerCrash('instance "' + arangod.role + '" Shutdown - ' + arangod.exitStatus.signal);
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
        serverCrashedLocal = true;
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
      if (a.role === b.role) return 0;
      if (a.role === 'coordinator' &&
          b.role === 'dbserver') return -1;
      if (b.role === 'coordinator' &&
          a.role === 'dbserver') return 1;
      if (a.role === 'agent') return 1;
      if (b.role === 'agent') return -1;
      return 0;
    });

    if (!this.options.noStartStopLogs) {
      print(Date() + ' Shutdown order ' + JSON.stringify(toShutdown));
    }

    let nonAgenciesCount = this.arangods.filter(arangod => {
      if (arangod.hasOwnProperty('exitStatus') &&
          (arangod.exitStatus.status !== 'RUNNING')) {
        return false;
      }
      return arangod.isRole(instanceRole.agent);
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
            print(Date() + ' forcefully terminating ' + yaml.safeDump(arangod.getStructure()) +
                  ' after ' + timeout + 's grace period; marking crashy.');
            serverCrashedLocal = true;
            shutdownSuccess = false;
            arangod.killWithCoreDump();
            arangod.analyzeServerCrash('shutdown timeout; instance "' +
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
            arangod.analyzeServerCrash('instance "' + arangod.role + '" Shutdown - ' + arangod.exitStatus.signal);
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

  checkClusterAlive() {
    let httpOptions = _.clone(this.httpAuthOptions)
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
            if (!arangod.hasOwnProperty('exitStatus') ||
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
      if (!arangod.isRole(instanceRole.agent) &&
          !arangod.isRole(instanceRole.single)) {
        let reply;
        try {
          httpOptions.method = 'GET';
          reply = download(arangod.url + '/_db/_system/_admin/server/id', '', httpOptions);
        } catch (e) {
          print(RED + Date() + " error requesting server '" + JSON.stringify(arangod) + "' Error: " + JSON.stringify(e) + RESET);
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

    if ((this.options.cluster || this.options.agency) &&
        !this.hasOwnProperty('clusterHealthMonitor') &&
        !this.options.disableClusterMonitor) {
      print("spawning cluster health inspector");
      internal.env.INSTANCEINFO = JSON.stringify(this);
      internal.env.OPTIONS = JSON.stringify(this.options);
      let args = makeArgsArangosh(this.options);
      args['javascript.execute'] = fs.join('js', 'client', 'modules', '@arangodb', 'testutils', 'clusterstats.js');
      const argv = toArgv(args);
      this.clusterHealthMonitor = executeExternal(pu.ARANGOSH_BIN, argv);
      this.clusterHealthMonitorFile = fs.join(instanceInfo.rootDir, 'stats.jsonl');
    }
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
      arango.reconnect(this.endpoint, '_system', 'root', '');
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
    if (!this.options.cluster) {
      let httpOptions = _.clone(this.httpAuthOptions)
      httpOptions.method = 'POST';
      httpOptions.returnBodyOnError = true;
      let count = 0;
      this.arangods.forEach(arangod => {
        if (arangod.suspended) {
          return;
        }
        while (true) {
          wait(1, false);
          if (this.options.useReconnect) {
            try {
              print(Date() + " reconnecting " + arangod.url);
              arango.reconnect(this.endpoint,
                               '_system',
                               this.options.username,
                               this.options.password,
                               count > 50
                              );
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
    if (!this.options.disableClusterMonitor) {
      this.initProcessStats();
    }
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

    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      if (oneInstance.isRole(instanceRole.primary) ||
          oneInstance.isRole(instanceRole.dbserver)) {
        print("relaunching: " + JSON.stringify(oneInstance.getStructure()));
        oneInstance.launchInstance(moreArgs);
      }
    });
    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      if (oneInstance.isRole(instanceRole.coordinator)) {
        print("relaunching: " + JSON.stringify(oneInstance.getStructure()));
        oneInstance.launchInstance(moreArgs);
      }
    });
    this.arangods.forEach(function (oneInstance, i) {
      if (oneInstance.pid) {
        return;
      }
      if (oneInstance.isRole(instanceRole.single)) {
        launchInstance(options, oneInstance, moreArgs);
      }
    });

    if (this.options.cluster && !this.options.skipReconnect) {
      this.checkClusterAlive({}); // todo addArgs
      print("reconnecting " + this.endpoint);
      arango.reconnect(instanceInfo.endpoint,
                       '_system',
                       options.username,
                       options.password,
                       false
                      );
    }
    this.launchFinalize(startTime);
  }
}

exports.instanceManager = instanceManager;
