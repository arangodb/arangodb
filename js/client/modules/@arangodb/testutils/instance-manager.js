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
const crashUtils = require('@arangodb/testutils/crash-utils');
const {versionHas, debugGetFailurePoints} = require("@arangodb/test-helper");
const crypto = require('@arangodb/crypto');
const ArangoError = require('@arangodb').ArangoError;;
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

let instanceCount = 1;

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
    this.agencyConfig = new inst.agencyConfig(options, this);
    this.dumpedAgency = false;
    this.leader = null;
    this.urls = [];
    this.endpoints = [];
    this.arangods = [];
    this.restKeyFile = '';
    this.tcpdump = null;
    this.JWT = null;
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
      let oneArangod = new inst.instance(this.options, '', {}, {}, '', '', '', this.agencyConfig, this.tmpDir);
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

  getMemLayout () {
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
      this.getMemLayout();
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
                                             this.agencyConfig,
                                             this.tmpDir,
                                             this.memlayout[instanceRole.agent]));
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
                                             this.agencyConfig,
                                             this.tmpDir,
                                             this.memlayout[instanceRole.dbServer]));
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
                                             this.agencyConfig,
                                             this.tmpDir,
                                             this.memlayout[instanceRole.coordinator] ));
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
                                             this.agencyConfig,
                                             this.tmpDir,
                                             this.memlayout[instanceRole.single]));
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

  nonfatalAssertSearch() {
    this.expectAsserts = true;
  }
  launchInstance() {
    if (this.options.hasOwnProperty('server')) {
      print("external server configured - not testing readyness! " + this.options.server);
      return;
    }

    internal.env['INSTANCEINFO'] = JSON.stringify(this.getStructure());

    const startTime = time();
    try {
      let count = 0;
      this.arangods.forEach(arangod => {
        arangod.startArango();
        count += 1;
        if (this.options.agency &&
            count === this.agencyConfig.agencySize) {
          this.detectAgencyAlive();
        }
      });
      if (this.options.cluster) {
        this.checkClusterAlive();
      }
      this.launchFinalize(startTime);
      return true;
    } catch (e) {
      // disable watchdog
      let hasTimedOut = internal.SetGlobalExecutionDeadlineTo(0.0);
      if (hasTimedOut) {
        print(RED + Date() + ' Deadline reached! Forcefully shutting down!' + RESET);
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
        let newStats = arangod._getProcessStats();
        let myDeltaStats = {};
        for (let key in arangod.stats) {
          if (key.startsWith('sockstat_')) {
            myDeltaStats[key] = newStats[key];
          } else {
            myDeltaStats[key] = newStats[key] - arangod.stats[key];
          }
        }
        deltaStats[arangod.pid + '_' + arangod.instanceRole] = myDeltaStats;
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
    this.dumpedAgency = true;
    const dumpdir = fs.join(this.options.testOutputDirectory, `agencydump_${this.instanceCount}`);
    const zipfn = fs.join(this.options.testOutputDirectory, `agencydump_${this.instanceCount}.zip`);
    if (fs.isFile(zipfn)) {
      fs.remove(zipfn);
    };
    if (fs.exists(dumpdir)) {
      fs.list(dumpdir).forEach(file => {
        const fn = fs.join(dumpdir, file);
        if (fs.isFile(fn)) {
          fs.remove(fn);
        }
      });
    } else {
      fs.makeDirectory(dumpdir);
    }
    this.arangods.forEach((arangod) => {
      if (arangod.isAgent()) {
        if (!arangod.checkArangoAlive()) {
          print(Date() + " this agent is already dead: " + JSON.stringify(arangod.getStructure()));
        } else {
          print(Date() + " Attempting to dump Agent: " + JSON.stringify(arangod.getStructure()));
          arangod.dumpAgent( '/_api/agency/config', 'GET', 'agencyConfig', dumpdir);

          arangod.dumpAgent('/_api/agency/state', 'GET', 'agencyState', dumpdir);

          arangod.dumpAgent('/_api/agency/read', 'POST', 'agencyPlan', dumpdir);
        }
      }
    });
    let zipfiles = [];
    fs.list(dumpdir).forEach(file => {
      const fn = fs.join(dumpdir, file);
      if (fs.isFile(fn)) {
        zipfiles.push(file);
      }
    });
    print(`${CYAN}${Date()} Zipping ${zipfn}${RESET}`);
    fs.zipFile(zipfn, dumpdir, zipfiles);
    zipfiles.forEach(file => {
      fs.remove(fs.join(dumpdir, file));
    });
    fs.removeDirectory(dumpdir);
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

  getFromPlan(path) {
    let req = this.agencyConfig.agencyInstances[0].getAgent('/_api/agency/read', 'POST', `[["/arango/${path}"]]`);
    if (req.code !== 200) {
      throw new Error(`Failed to query agency [["/arango/${path}"]] : ${JSON.stringify(req)}`);
    }
    return JSON.parse(req["body"])[0];
  }

  removeServerFromAgency(serverId) {
    // Make sure we remove the server
    for (let i = 0; i < 10; ++i) {
      const res = arango.POST_RAW("/_admin/cluster/removeServer", JSON.stringify(serverId));
      if (res.code === 404 || res.code === 200) {
        // Server is removed
        return;
      }
      // Server could not be removed, give supervision some more time
      // and then try again.
      print("Wait for supervision to clear responsibilty of server");
      require("internal").wait(0.2);
    }
    // If we reach this place the server could not be removed
    // it is still responsible for shards, so a failover
    // did not work out.
    throw "Could not remove shutdown server";
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
      let timeout = internal.SetGlobalExecutionDeadlineTo(0.0);
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
      const seconds = x => x * 1000;
      const checkAllAlive = () => this.arangods.every(arangod => arangod.checkArangoAlive());
      let first = true;
      rc = false;
      for (
        const start = Date.now();
        !rc && Date.now() < start + seconds(60) && checkAllAlive();
      ) {
        rc = this._checkServersGOOD();
        if (first) {
          if (!this.options.noStartStopLogs) {
            print(RESET + "Waiting for all servers to go GOOD...");
          }
          first = false;
        }
        if (!rc) {
          internal.sleep(1);
        }
      }
    }
    if (!rc) {
      this.dumpAgency();
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

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief checks whether any instance has failure points set
  // //////////////////////////////////////////////////////////////////////////////

  checkServerFailurePoints(instanceMgr = null) {
    let failurePoints = [];
    let im = this;
    if (instanceMgr !== null) {
      im = instanceMgr;
    }

    im.arangods.forEach(arangod => {
      // we don't have JWT success atm, so if, skip:
      if ((!arangod.isAgent()) &&
          !arangod.args.hasOwnProperty('server.jwt-secret-folder') &&
          !arangod.args.hasOwnProperty('server.jwt-secret')) {
        let fp = debugGetFailurePoints(arangod.endpoint);
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

  detectShouldBeRunning() {
    let ret = true;
    this.arangods.forEach(arangod => { ret = ret && arangod.pid !== null; } );
    return ret;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief shuts down an instance
  // //////////////////////////////////////////////////////////////////////////////

  shutdownInstance (forceTerminate, moreReason="") {
    if (forceTerminate === undefined) {
      forceTerminate = false;
    }
    let timeoutReached = internal.SetGlobalExecutionDeadlineTo(0.0);
    if (timeoutReached) {
      print(RED + Date() + ' Deadline reached! Forcefully shutting down!' + RESET);
      moreReason += "Deadline reached! ";
      this.arangods.forEach(arangod => { arangod.serverCrashedLocal = true;});
      forceTerminate = true;
    }
    if (forceTerminate && !timeoutReached && this.options.agency && !this.dumpedAgency) {
      // the SUT is unstable, but we want to attempt an agency dump anyways.
      try {
        print(CYAN + Date() + ' attempting to dump agency in forceterminate!' + RESET);
        // set us a short deadline so we don't get stuck.
        internal.SetGlobalExecutionDeadlineTo(this.options.oneTestTimeout * 100);
        this.dumpAgency();
      } catch(ex) {
        print(CYAN + Date() + ' aborted to dump agency in forceterminate! ' + ex + RESET);
      } finally {
        internal.SetGlobalExecutionDeadlineTo(0.0);
      }
    }
    try {
      if (forceTerminate) {
        return this._forceTerminate(moreReason);
      } else {
        return this._shutdownInstance();
      }
    }
    catch (e) {
      if (e instanceof ArangoError && e.errorNum === internal.errors.ERROR_DISABLED.code) {
        let timeoutReached = internal.SetGlobalExecutionDeadlineTo(0.0);
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

    if ((this.options.cluster || this.options.agency) && this.hasOwnProperty('clusterHealthMonitor')) {
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
    if (forceTerminate) {
      return this._forceTerminate(moreReason);
    }

    var shutdownTime = internal.time();

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
          if ((internal.time() - shutdownTime) > localTimeout) {
            print(Date() + ' forcefully terminating ' + yaml.safeDump(arangod.getStructure()) +
                  ' after ' + timeout + 's grace period; marking crashy.');
            this.dumpAgency();
            arangod.serverCrashedLocal = true;
            shutdownSuccess = false;
            arangod.killWithCoreDump(`taking coredump because of timeout ${internal.time()} - ${shutdownTime}) > ${localTimeout} during shutdown`);
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
      arangod.readAssertLogLines(this.expectAsserts);
      if (arangod.exitStatus && arangod.exitStatus.exit !== 0) {
        print(RED + `arangod "${arangod.instanceRole}" with pid ${arangod.pid} exited with exit code ${arangod.exitStatus.exit}` + RESET);
        shutdownSuccess = false;
      }
    });
    this.cleanup = this.cleanup && shutdownSuccess;
    return shutdownSuccess;
  }

  getAgency(path, method, body = null) {
    return this.agencyConfig.agencyInstances[0].getAgent(path, method, body);
  }

  detectAgencyAlive() {
    let count = 20;
    while (count > 0) {
      let haveConfig = 0;
      let haveLeader = 0;
      let leaderId = null;
      for (let agentIndex = 0; agentIndex < this.agencyConfig.agencySize; agentIndex ++) {
        let reply = this.agencyConfig.agencyInstances[agentIndex].getAgent('/_api/agency/config', 'GET');
        if (this.options.extremeVerbosity) {
          print("Response ====> ");
          print(reply);
        }
        if (!reply.error && reply.code === 200) {
          let res = JSON.parse(reply.body);
          if (res.hasOwnProperty('lastAcked')){
            haveLeader += 1;
          }
          if (res.hasOwnProperty('leaderId') && res.leaderId !== "") {
            haveConfig += 1;
            if (leaderId === null) {
              leaderId = res.leaderId;
            } else if (leaderId !== res.leaderId) {
              haveLeader = 0;
              haveConfig = 0;
            }
          }
        }
        if (haveLeader === 1 && haveConfig === this.agencyConfig.agencySize) {
          print("Agency Up!");
          try {
            // set back log level to info for agents
            for (let agentIndex = 0; agentIndex < this.agencyConfig.agencySize; agentIndex ++) {
              this.agencyConfig.agencyInstances[agentIndex].getAgent('/_admin/log/level', 'PUT', JSON.stringify({"agency":"info"}));
            }
          } catch (err) {}
          return;
        }
        if (count === 0) {
          throw new Error("Agency didn't come alive in time!");
        }
        sleep(0.5);
        count --;
      }
    }
  }

  detectCurrentLeader() {
    let opts = {
      method: 'POST',
      jwt: crypto.jwtEncode(this.arangods[0].args['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256'),
      headers: {'content-type': 'application/json' }
    };
    let count = 60;
    while ((count > 0) && (this.agencyConfig.agencyInstances[0].pid !== null)) {
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
          throw new Error("Leader is not selected");
        }
        sleep(0.5);
      }
    }
    this.leader = null;
    while (this.leader === null) {
      this.urls.forEach(url => {
        this.arangods.forEach(arangod => {
          if ((arangod.url === url && arangod.pid === null)) {
            throw new Error("detectCurrentleader: instance we attempt to query not alive");
          }});
        opts['method'] = 'GET';
        let reply = download(url + '/_api/cluster/endpoints', '', opts);
        if (reply.code === 200) {
          let res;
          try {
            res = JSON.parse(reply.body);
          }
          catch (x) {
            throw new Error("Failed to parse endpoints reply: " + JSON.stringify(reply));
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
          arangod.upAndRunning = true;
          return true;
        }
        try {
          if (reply.code === 403) {
            let parsedBody = JSON.parse(reply.body);
            if (parsedBody.errorNum === internal.errors.ERROR_SERVICE_API_DISABLED.code) {
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
        internal.sleep(5);
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
                let status = arangod.status(false);
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
          if (count === this.startupMaxCount) {
            throw new Error('startup timed out! bailing out!');
          }
        }
      });
      this.endpoints = [this.endpoint];
      this.urls = [this.url];
    } else if (this.options.agency && !this.options.cluster) {
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
exports.registerOptions = function(optionsDefaults, optionsDocumentation, optionHandlers) {
  tu.CopyIntoObject(optionsDefaults, {
    'memory': undefined,
    'singles': 1, // internal only
    'dumpAgencyOnError': true,
    'agencySize': 3,
    'agencyWaitForSync': false,
    'agencySupervision': true,
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
    '   - `agency`: if set to true agency tests are done',
    '   - `agencySize`: number of agents in agency',
    '   - `agencySupervision`: run supervision in agency',
    '   - `extraArgs`: list of extra commandline arguments to add to arangod',
    ' SUT monitoring',
    '   - `sleepBeforeStart` : sleep at tcpdump info - use this to dump traffic or attach debugger',
    '   - `dumpAgencyOnError`: if we should create an agency dump if an error occurs',
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
