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

const crypto = require('@arangodb/crypto');
const _ = require("lodash");
const internal = require("internal");
const fs = require('fs');
const tu = require('@arangodb/testutils/test-utils');
const pu = require('@arangodb/testutils/process-utils');
const jsunity = require('jsunity');
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const arangosh = require('@arangodb/arangosh');
const request = require('@arangodb/request');
const isArm = require("@arangodb/test-helper-common").versionHas('arm');

const {
  arango,
  db,
  download,
  time,
  sleep,
  wait
} = internal;
// const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = internal.COLORS.COLOR_YELLOW;
const seconds = x => x * 1000;

const shardIdToLogId = function (shardId) {
  return shardId.slice(1);
};

class agencyMgr {
  constructor(options, wholeCluster) {
    this.options = options;
    this.wholeCluster = wholeCluster;
    this.agencySize = options.agencySize;
    this.supervision = String(options.agencySupervision);
    this.waitForSync = false;
    if (options.agencyWaitForSync !== undefined) {
      this.waitForSync = options.agencyWaitForSync;
    }
    this.agencyInstances = [];
    this.agencyEndpoint = "";
    this.agentsLaunched = 0;
    this.urls = [];
    this.endpoints = [];
    this.leader = null;
    this.dumpedAgency = false;
  }
  getStructure() {
    return {
      agencySize: this.agencySize,
      agencyInstances: this.agencyInstances.length,
      supervision: this.supervision,
      waitForSync: this.waitForSync,
      agencyEndpoint: this.agencyEndpoint,
      agentsLaunched: this.agentsLaunched,
      urls: this.urls,
      endpoints: this.endpoints
    };
  }
  setFromStructure(struct) {
    this.agencySize = struct['agencySize'];
    this.supervision = struct['supervision'];
    this.waitForSync = struct['waitForSync'];
    this.agencyEndpoint = struct['agencyEndpoint'];
    this.agentsLaunched = struct['agentsLaunched'];
    this.urls = struct['urls'];
    this.endpoints = struct['endpoints'];
  }
  waitFor(checkFn, maxTries = 240, onErrorCallback) {
    const waitTimes = [0.1, 0.1, 0.2, 0.2, 0.2, 0.3, 0.5];
    const getWaitTime = (count) => waitTimes[Math.min(waitTimes.length-1, count)];
    let count = 0;
    let result = null;
    while (count < maxTries) {
      result = checkFn(this);
      if (result === true || result === undefined) {
        return result;
      }
      if (!(result instanceof Error)) {
        throw Error("expected error");
      }
      count += 1;
      if (count % 10 === 0) {
        console.log(result);
      }
      wait(getWaitTime(count));
    }
    if (onErrorCallback !== undefined) {
      onErrorCallback(result);
    } else {
      throw result;
    }
  }

  getDbServers() {
    return this.wholeCluster.arangods.filter(arangod => arangod.isRole("dbserver")).map((x) => x.id);
  }

  getAgency(path, method, body = null) {
    return this.getAnyAgent(this.agencyInstances[0], path, method, body);
  }

  shouldBeCompleted() {
    let count = 0;
    this.agencyInstances.forEach(agent => { if (agent.pid !== null) {count ++;}});
    return count === this.agencySize;
  }

  moreIsAlreadyRunning() {
    let count = 0;
    this.wholeCluster.arangods.forEach(arangod => { if (arangod.pid !== null) {count ++;}});
    return count > this.agencySize;
  }
  getAnyAgent(agent, path, method, body = null) {
    while(true) {
      let opts = {};
      if (body === null) {
        body = (method === 'POST') ? '[["/"]]' : '';
      }
      if (agent.JWT) {
        opts = pu.makeAuthorizationHeaders(agent.options, agent.args, agent.JWT);
      } else {
        let allArgs = [agent.args, agent.moreArgs];
        allArgs.forEach(args => {
          if (allArgs.hasOwnProperty('authOpts')) {
            opts['jwt'] = crypto.jwtEncode(agent.authOpts['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
          } else if (allArgs.hasOwnProperty('server.jwt-secret')) {
            opts['jwt'] = crypto.jwtEncode(args['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
          } else if (agent.jwtFiles) {
            opts['jwt'] = crypto.jwtEncode(fs.read(agent.jwtFiles[0]), {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
          }
        });
      }
      opts['method'] = method;
      opts['returnBodyOnError'] = true;
      opts['followRedirects'] = false;
      let ret = download(agent.url + path, body, opts);
      if (ret.code !== 307 && ret.code !== 303) {
        return ret;
      }
      try {
        let newAgentUrl = ret.headers['location'];
        agent = this.agencyInstances.filter(agent => { return newAgentUrl.search(agent.url) === 0;})[0];
      } catch (ex) {
        throw new Error(`couldn't find agent to redirect to ${JSON.stringify(ret)}`);
      }
      print(`following redirect to ${agent.name}`);
    }
  }
  postAgency(operation, body = null) {
    let res = this.getAnyAgent(this.agencyInstances[0],
                               `/_api/agency/${operation}`,
                               'POST',
                               JSON.stringify(body));
    assertTrue(res.hasOwnProperty('code'), JSON.stringify(res));
    assertEqual(res.code, 200, JSON.stringify(res));
    assertTrue(res.hasOwnProperty('message'));
    return arangosh.checkRequestResult(JSON.parse(res.body));
  }
  call(operation, body) {
    return this.postAgency(operation, body);
  }

  get(key) {
    const res = this.postAgency( 'read', [[
      `/arango/${key}`,
    ]]);
    return res[0];
  }
  unWrapQueriedItem(key, res) {
    const path = ['arango', ...key.split('/').filter(i => i)];
    for (const p of path) {
      if (res === undefined) {
        return undefined;
      }
      res = res[p];
    }
    return res;
  }
  getAt(key) {
    return this.unWrapQueriedItem(
      key,
      this.postAgency( 'read', [[
        `/arango/${key}`,
      ]])[0]);
  }
  set(path, value) {
    return this.postAgency('write', [[{
      [`/arango/${path}`]: {
        'op': 'set',
        'new': value,
      },
    }]]);
  }
  remove(path) {
    return this.postAgency('write', [[{
      [`/arango/${path}`]: {
        'op': 'delete'
      },
    }]]);
  }
  write(body) {
    return this.postAgency("write", body);
  }
  transact(body) {
    return this.postAgency("transact", body);
  }
  increaseVersion(path) {
    return this.postAgency('write', [[{
      [`/arango/${path}`]: {
        'op': 'increment',
      },
    }]]);
  }
  casValue(key, deltaValue) {
    let keyStr = `/arango/${key}`;
    while (true) {
      let oldValue = this.getAt(key);
      try {
        let ret = this.write([[
          {
            [[keyStr]]: {
              'op': 'set',
              'new': oldValue + deltaValue,
            },
          },
          {
            [[keyStr]]: {
              'old': oldValue
            }
          }
        ]]);
        return oldValue;
      } catch (ex) {
        print(`request to casValue for ${key} failed: ${ex}, will retry`);
      }
    }
  }
  uniqId(deltaValue) {
    return this.casValue('Sync/LatestID', deltaValue);
  }
  delaySupervisionFailoverActions(value) {
    this.agencyInstances.forEach(agent => {
      let res = this.getAnyAgent(agent,
                                 "/_api/agency/config", 'PUT',
                                 JSON.stringify(
                                   {
                                     delayAddFollower: value,
                                     delayFailedFollower: value
                                   }));
      assertEqual(200, res.code);
    });
  }



  getServerRebootId (serverId) {
    return this.getAt(`Current/ServersKnown/${serverId}/rebootId`);
  }

  isDBServerInCurrent(serverId) {
    // TODO: not validated.
    return this.getAt(`Current/DBServers/${serverId}`);
  }

  bumpServerRebootId(serverId) {
    const response = this.increaseVersion(`Current/ServersKnown/${serverId}/rebootId`);
    if (response !== true) {
      return undefined;
    }
    return this.getServerRebootId(serverId);
  };


  checkServerHealth(serverId, expectValue) {
    return this.getAt(`Supervision/Health/${serverId}/Status`) === expectValue;
  }

  serverHealthy(serverId) {
    return this.checkServerHealth(serverId, "GOOD");
  };
  serverFailed(serverId) {
    return this.checkServerHealth(serverId, "FAILED");
  }

/**
 * @param {string} database
 * @param {string} logId
 * @returns {{
 *       target: {
 *         id: number,
 *         leader?: string,
 *         participants: Object<string, {
 *           forced: boolean,
 *           allowedInQuorum: boolean,
 *           allowedAsLeader: boolean
 *         }>,
 *         config: {
 *           writeConcern: number,
 *           softWriteConern: number,
 *           replicationFactor: number,
 *           waitForSync: boolean
 *         },
 *         properties: {}
 *       },
 *       plan: {
 *         id: number,
 *         participantsConfig: {
 *           participants: Object<string, {
 *             forced: boolean,
 *             allowedInQuorum: boolean,
 *             allowedAsLeader: boolean
 *           }>,
 *           generation: number,
 *           config: {
 *             waitForSync: boolean,
 *             effectiveWriteConcern: number
 *           }
 *         },
 *         currentTerm?: {
 *           term: number,
 *           leader?: {
 *             serverId: string,
 *             rebootId: number
 *           }
 *         }
 *       },
 *       current: {
 *         localStatus: Object<string, Object>,
 *         localState: Object,
 *         supervision?: Object,
 *         leader?: Object,
 *         safeRebootIds?: Object<string, number>
 *       }
 *   }}
 */
  readReplicatedLogAgency(database, logId) {
    let target = this.getAt(`Target/ReplicatedLogs/${database}/${logId}`);
    let plan = this.getAt(`Plan/ReplicatedLogs/${database}/${logId}`);
    let current = this.getAt(`Current/ReplicatedLogs/${database}/${logId}`);
    return {target, plan, current};
  }

  replicatedLogIsReady (database, logId, term, participants, leader) {
    return function () {
      const {current, plan} = this.readReplicatedLogAgency(database, logId);
      if (plan === undefined) {
        return Error("plan not yet defined");
      }
      if (current === undefined) {
        return Error("current not yet defined");
      }
      const electionTerm = !plan.currentTerm.hasOwnProperty('leader');
      // If there's no leader, followers will stay in "Connecting".
      const readyState = electionTerm ? 'Connecting' : 'ServiceOperational';

      for (const srv of participants) {
        if (!current.localStatus || !current.localStatus[srv]) {
          return Error(`Participant ${srv} has not yet reported to current.`);
        }
        if (current.localStatus[srv].term < term) {
          return Error(`Participant ${srv} has not yet acknowledged the current term; ` +
                       `found = ${current.localStatus[srv].term}, expected = ${term}.`);
        }
        if (current.localStatus[srv].state !== readyState) {
          return Error(`Participant ${srv} state not yet ready, found  ${current.localStatus[srv].state}` +
                       `, expected = "${readyState}".`);
        }
      }

      if (leader !== undefined) {
        if (!current.leader) {
          return Error("Leader has not yet established its term");
        }
        if (current.leader.serverId !== leader) {
          return Error(`Wrong leader in current; found = ${current.leader.serverId}, expected = ${leader}`);
        }
        if (current.leader.term < term) {
          return Error(`Leader has not yet confirmed the term; found = ${current.leader.term}, expected = ${term}`);
        }
        if (!current.leader.leadershipEstablished) {
          return Error("Leader has not yet established its leadership");
        }
      }
      return true;
    };
  }

  replicatedLogLeaderEstablished(database, logId, term, participants) {
    return function (inst) {
      let {plan, current} = inst.readReplicatedLogAgency(database, logId);
      if (current === undefined) {
        return Error("current not yet defined");
      }
      if (plan === undefined) {
        // This may seem strange, but it's actually needed. Due to supervision logging actions to Current,
        // we can end up with an entry in Current, before the Plan is created.
        return Error("plan not yet defined");
      }

      for (const srv of participants) {
        if (!current.localStatus || !current.localStatus[srv]) {
          return Error(`Participant ${srv} has not yet reported to current.`);
        }
        if (term !== undefined && current.localStatus[srv].term < term) {
          return Error(`Participant ${srv} has not yet acknowledged the current term; ` +
                       `found = ${current.localStatus[srv].term}, expected = ${term}.`);
        }
      }

      if (term === undefined) {
        if (!plan.currentTerm) {
          return Error(`No term in plan`);
        }
        term = plan.currentTerm.term;
      }

      if (!current.leader || current.leader.term < term) {
        return Error("Leader has not yet established its term");
      }
      if (!current.leader.leadershipEstablished) {
        return Error("Leader has not yet established its leadership");
      }

      return true;
    };
  }

  replicatedLogSetPlanParticipantsConfig(database, logId, participantsConfig) {
    this.set(`Plan/ReplicatedLogs/${database}/${logId}/participantsConfig`, participantsConfig);
    this.increaseVersion(`Plan/Version`);
  }

  replicatedLogSetTargetParticipantsConfig(database, logId, participantsConfig) {
    this.set(`Target/ReplicatedLogs/${database}/${logId}/participants`, participantsConfig);
    this.increaseVersion(`Target/Version`);
  }

  replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, participants) {
    const oldValue = this.get(`Plan/ReplicatedLogs/${database}/${logId}/participantsConfig`);
    const newValue = oldValue || {generation: 0, participants: {}};
    for (const [p, v] of Object.entries(participants)) {
      if (v === null) {
        delete newValue.participants[p];
      } else {
        newValue.participants[p] = v;
      }
    }
    const gen = newValue.generation += 1;
    this.replicatedLogSetPlanParticipantsConfig(database, logId, newValue);
    return gen;
  }

  replicatedLogUpdateTargetParticipants(database, logId, participants) {
    const oldValue = this.get(`Target/ReplicatedLogs/${database}/${logId}/participants`);
    const newValue = oldValue || {};
    for (const [p, v] of Object.entries(participants)) {
      if (v === null) {
        delete newValue[p];
      } else {
        newValue[p] = v;
      }
    }
    this.replicatedLogSetTargetParticipantsConfig(database, logId, newValue);
  }
  getParticipantsObjectForServers(servers) {
    _.reduce(servers, (a, v) => {
      a[v] = {allowedInQuorum: true, allowedAsLeader: true, forced: false};
      return a;
    }, {});
  }

  createParticipantsConfig(generation, config, servers) {
    return {
      generation,
      config,
      participants: this.getParticipantsObjectForServers(servers),
    };
  }

  createTermSpecification(term, servers, leader) {
    let spec = {term};
    if (leader !== undefined) {
      if (!_.includes(servers, leader)) {
        throw Error("leader is not part of the participants");
      }
      spec.leader = {serverId: leader, rebootId: this.getServerRebootId(leader)};
    }
    return spec;
  }

  replicatedLogSetPlanTerm(database, logId, term) {
    this.set(`Plan/ReplicatedLogs/${database}/${logId}/currentTerm/term`, term);
    this.increaseVersion(`Plan/Version`);
  }

  triggerLeaderElection(database, logId) {
    // This operation has to be in one envelope. Otherwise we violate the assumption
    // that they are only modified as a unit.
    this.transact([[{
      [`/arango/Plan/ReplicatedLogs/${database}/${logId}/currentTerm/term`]: {
        'op': 'increment',
      },
      [`/arango/Plan/ReplicatedLogs/${database}/${logId}/currentTerm/leader`]: {
        'op': 'delete'
      }
    }]]);
    this.increaseVersion(`Plan/Version`);
  }

  replicatedLogSetPlanTermConfig(database, logId, term) {
    this.set(`Plan/ReplicatedLogs/${database}/${logId}/currentTerm`, term);
    this.increaseVersion(`Plan/Version`);
  }

  replicatedLogSetPlan(database, logId, spec) {
    this.set(`Plan/ReplicatedLogs/${database}/${logId}`, spec);
    this.increaseVersion(`Plan/Version`);
  }

  replicatedLogSetTarget(database, logId, spec) {
    this.set(`Target/ReplicatedLogs/${database}/${logId}`, spec);
    this.increaseVersion(`Target/Version`);
  }

  replicatedLogDeletePlan(database, logId) {
    this.remove(`Plan/ReplicatedLogs/${database}/${logId}`);
    this.increaseVersion(`Plan/Version`);
  }

  replicatedLogDeleteTarget(database, logId) {
    this.remove(`Target/ReplicatedLogs/${database}/${logId}`);
    this.increaseVersion(`Target/Version`);
  }

  createReconfigureJob(database, logId, ops) {
    const jobId = this.nextUniqueLogId();
    this.set(`Target/ToDo/${jobId}`, {
      type: "reconfigureReplicatedLog",
      database, logId, jobId: "" + jobId,
      operations: ops,
    });
    return jobId;
  }

  registerAgencyTestBegin(testName) {
    this.set(`Testing/${testName}/Begin`, (new Date()).toISOString());
  }

  registerAgencyTestEnd(testName) {
    this.set(`Testing/${testName}/End`, (new Date()).toISOString());
  }

  getReplicatedLogLeaderPlan(database, logId, nothrow = false) {
    let {plan} = this.readReplicatedLogAgency(database, logId);
    if (!plan.currentTerm) {
      const error = Error("no current term in plan");
      if (nothrow) {
        return error;
      }
      throw error;
    }
    if (!plan.currentTerm.leader) {
      const error = Error("current term has no leader");
      if (nothrow) {
        return error;
      }
      throw error;
    }
    const leader = plan.currentTerm.leader.serverId;
    const rebootId = plan.currentTerm.leader.rebootId;
    const term = plan.currentTerm.term;
    return {leader, term, rebootId};
  }

  getReplicatedLogLeaderTarget(database, logId) {
    let {target} = this.readReplicatedLogAgency(database, logId);
    return target.leader;
  }


  createReplicatedLogPlanOnly(database, targetConfig, replicationFactor) {
    const logId = this.nextUniqueLogId();
    const servers = _.sampleSize(this.getDbServers(), replicationFactor);
    const leader = servers[0];
    const term = 1;
    const generation = 1;
    this.replicatedLogSetPlan(database, logId, {
      id: logId,
      currentTerm: this.createTermSpecification(term, servers, leader),
      participantsConfig: this.createParticipantsConfig(generation, targetConfig, servers),
      properties: {implementation: {type: "black-hole", parameters: {}}}
    });

    // wait for all servers to have reported in current
    this.waitFor(this.replicatedLogIsReady(database, logId, term, servers, leader));
    const followers = _.difference(servers, [leader]);
    const remaining = _.difference(this.getDbServers(), servers);
    return {logId, servers, leader, term, followers, remaining};
  }

  createReplicatedLogInTarget(database, targetConfig, replicationFactor, servers) {
    const logId = this.nextUniqueLogId();
    if (replicationFactor === undefined) {
      replicationFactor = 3;
    }
    if (servers === undefined) {
      servers = _.sampleSize(this.getDbServers(), replicationFactor);
    }
    this.replicatedLogSetTarget(database, logId, {
      id: logId,
      config: targetConfig,
      participants: this.getParticipantsObjectForServers(servers),
      supervision: {maxActionsTraceLength: 20},
      properties: {implementation: {type: "black-hole", parameters: {}}}
    });

    this.waitFor(() => {
      let {plan, current} = this.readReplicatedLogAgency(database, logId.toString());
      if (current === undefined) {
        return Error("current not yet defined");
      }
      if (plan === undefined) {
        return Error("plan not yet defined");
      }

      if (!plan.currentTerm) {
        return Error(`No term in plan`);
      }

      if (!current.leader) {
        return Error("Leader has not yet established its term");
      }
      if (!current.leader.leadershipEstablished) {
        return Error("Leader has not yet established its leadership");
      }

      return true;
    });

    const {leader, term} = this.getReplicatedLogLeaderPlan(database, logId);
    const followers = _.difference(servers, [leader]);
    return {logId, servers, leader, term, followers};
  }

  createReplicatedLog(database, targetConfig, replicationFactor) {
    const logId = this.nextUniqueLogId();
    if (replicationFactor === undefined) {
      replicationFactor = 3;
    }
    const servers = _.sampleSize(this.getDbServers(), replicationFactor);
    this.replicatedLogSetTarget(database, logId, {
      id: logId,
      config: targetConfig,
      participants: this.getParticipantsObjectForServers(servers),
      supervision: {maxActionsTraceLength: 20},
      properties: {implementation: {type: "black-hole", parameters: {}}}
    });

    this.waitFor(this.replicatedLogLeaderEstablished(database, logId, undefined, servers));

    const {leader, term} = this.getReplicatedLogLeaderPlan(database, logId);
    const followers = _.difference(servers, [leader]);
    return {logId, servers, leader, term, followers};
  }

  createReplicatedLogWithState(database, targetConfig, stateType, replicationFactor) {
    const logId = this.nextUniqueLogId();
    if (replicationFactor === undefined) {
      replicationFactor = 3;
    }
    const servers = _.sampleSize(this.getDbServers(), replicationFactor);
    this.replicatedLogSetTarget(database, logId, {
      id: logId,
      config: targetConfig,
      participants: this.getParticipantsObjectForServers(servers),
      supervision: {maxActionsTraceLength: 20},
      properties: {implementation: {type: stateType, parameters: {}}}
    });

    this.waitFor(this.replicatedLogLeaderEstablished(database, logId, undefined, servers));

    const {leader, term} = this.getReplicatedLogLeaderPlan(database, logId);
    const followers = _.difference(servers, [leader]);
    return {logId, servers, leader, term, followers};
  }

  dumpAgent(agent, path, method, fn, dumpdir) {
    print('--------------------------------- '+ fn + ' -----------------------------------------------');
    let agencyReply = this.getAnyAgent(agent, path, method);
    if (agencyReply.code === 200) {
      if (fn === "agencyState") {
        fs.write(fs.join(dumpdir, `${fn}_${agent.pid}.json`), agencyReply.body);
      } else {
        let agencyValue = JSON.parse(agencyReply.body);
        fs.write(fs.join(dumpdir, `${fn}_${agent.pid}.json`), JSON.stringify(agencyValue, null, 2));
      }
    } else {
      print(agencyReply);
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief dump the state of the agency to disk. if we still can get one.
  // //////////////////////////////////////////////////////////////////////////////
  dumpAgency() {
    if ((this.options.agency === false) || (this.options.dumpAgencyOnError === false) || this.dumpedAgency) {
      return;
    }

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
    this.agencyInstances.forEach(arangod => {
      if (!arangod.checkArangoAlive()) {
        print(Date() + " this agent is already dead: " + JSON.stringify(arangod.getStructure()));
      } else {
        print(Date() + " Attempting to dump Agent: " + JSON.stringify(arangod.getStructure()));
        this.dumpAgent(arangod,  '/_api/agency/config', 'GET', 'agencyConfig', dumpdir);

        this.dumpAgent(arangod, '/_api/agency/state', 'GET', 'agencyState', dumpdir);

        this.dumpAgent(arangod, '/_api/agency/read', 'POST', 'agencyPlan', dumpdir);
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
  getFromPlan(path) {
    let req = this.getAnyAgent(this.agencyInstances[0], '/_api/agency/read', 'POST', `[["/arango/${path}"]]`);
    if (req.code !== 200) {
      throw new Error(`Failed to query agency [["/arango/${path}"]] : ${JSON.stringify(req)}`);
    }
    return JSON.parse(req["body"])[0];
  }

  getShardsToLogsMapping(dbName, colId) {
    const colPlan = this.getAt(`Plan/Collections/${dbName}/${colId}`);
    let mapping = {};
    if (colPlan.hasOwnProperty("groupId")) {
      const groupId = colPlan.groupId;
      const shards = colPlan.shardsR2;
      const colGroup = this.getAt(`Plan/CollectionGroups/${dbName}/${groupId}`);
      const shardSheaves = colGroup.shardSheaves;
      for (let idx = 0; idx < shards.length; ++idx) {
        mapping[shards[idx]] = shardSheaves[idx].replicatedLog;
      }
    } else {
      // Legacy code, supporting system collections
      const shards = colPlan.shards;
      for (const [shardId, _] of Object.entries(shards)) {
        mapping[shardId] = shardIdToLogId(shardId);
      }
    }
    return mapping;
  }

  getCollectionShardsAndLogs(db, collection) {
    const shards = collection.shards();
    const shardsToLogs = this.getShardsToLogsMapping(db._name(), collection._id);
    const logs = shards.map(shardId => db._replicatedLog(shardsToLogs[shardId]));
    return {shards, shardsToLogs, logs};
  }

  /**
   * Causes underlying replicated logs to trigger leader recovery.
   */
  bumpTermOfLogsAndWaitForConfirmation(dbn, col) {
    const {numberOfShards, isSmart} = col.properties();
    if (isSmart && numberOfShards === 0) {
      // Adjust for SmartEdgeCollections
      col = db._collection(`_local_${col.name()}`);
    }
    const shards = col.shards();
    const shardsToLogs = this.getShardsToLogsMapping(dbn, col._id);
    const stateMachineIds = shards.map(s => shardsToLogs[s]);

    const increaseTerm = (stateId) => this.triggerLeaderElection(dbn, stateId);
    const clearOldLeader = (stateId) => this.wholeCluster.unsetLeader(dbn, stateId);

    // Clear the old leader, so it doesn't get back automatically.
    stateMachineIds.forEach(clearOldLeader);
    stateMachineIds.forEach(increaseTerm);
    const leaderReady = function (stateId, inst) {
      return inst.replicatedLogLeaderEstablished(dbn, stateId, undefined, []);
    };

    stateMachineIds.forEach(x => this.waitFor(leaderReady(x, this)));
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

  dumpAgencyIfNotYet(forceTerminate, timeoutReached) {
    if (forceTerminate && !timeoutReached && !this.dumpedAgency) {
      // the SUT is unstable, but we want to attempt an agency dump anyways.
      try {
        print(CYAN + Date() + ' attempting to dump agency in forceterminate!' + RESET);
        // set us a short deadline so we don't get stuck.
        internal.SetGlobalExecutionDeadlineTo(this.options.oneTestTimeout / 10);
        this.dumpAgency();
      } catch(ex) {
        print(CYAN + Date() + ' aborted to dump agency in forceterminate! ' + ex + RESET);
      } finally {
        internal.SetGlobalExecutionDeadlineTo(0.0);
      }
    }
  }
  
  detectAgencyAlive(httpAuthOptions) {
    if (!this.options.agency ||
        !this.shouldBeCompleted() ||
        this.moreIsAlreadyRunning()) {
      print("no agency check this time.");
      return;
    }
    let count = (isArm || this.options.isInstrumented) ? 75 : 30;
    while (count > 0) {
      let haveConfig = 0;
      let haveLeader = 0;
      let leaderId = null;
      for (let agentIndex = 0; agentIndex < this.agencySize; agentIndex ++) {
        let reply = this.getAnyAgent(this.agencyInstances[agentIndex], '/_api/agency/config', 'GET');
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
        if (haveLeader === 1 && haveConfig === this.agencySize) {
          print("Agency Up!");
          try {
            // set back log level to info for agents
            for (let agentIndex = 0; agentIndex < this.agencySize; agentIndex ++) {
              this.getAnyAgent(this.agencyInstances[agentIndex], '/_admin/log/level', 'PUT', JSON.stringify({"agency":"info"}));
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

}

exports.registerOptions = function(optionsDefaults, optionsDocumentation, optionHandlers) {
  tu.CopyIntoObject(optionsDefaults, {
    'dumpAgencyOnError': true,
    'agencySize': 3,
    'agencyWaitForSync': false,
    'agencySupervision': true,
  });

  tu.CopyIntoList(optionsDocumentation, [
    ' SUT agency configuration properties:',
    '   - `agency`: if set to true agency tests are done',
    '   - `agencySize`: number of agents in agency',
    '   - `agencySupervision`: run supervision in agency',
    '   - `dumpAgencyOnError`: if we should create an agency dump if an error occurs',
    ''
  ]);
};
exports.agencyMgr = agencyMgr;
