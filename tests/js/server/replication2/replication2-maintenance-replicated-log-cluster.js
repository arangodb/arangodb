/*jshint strict: true */
/*global assertEqual, assertTrue, fail, ArangoClusterInfo */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const _ = require('lodash');
const db = arangodb.db;
const internal = require("internal");
const wait = internal.wait;
const print = internal.print;

const database = '_system';

const readAgencyAtKey = function (key) {
    const response = global.ArangoAgency.get(key);
    const path = ["arango", ...key.split('/')];
    let result = response;
    for (const p of path) {
        if (result === undefined) {
            return undefined;
        }
        result = result[p];
    }
    return result;
};

const writeAgencyAtKey = function (key, value) {
    global.ArangoAgency.set(key, value);
};

const readReplicatedLogAgency = function (logId) {
    let target = readAgencyAtKey(`Target/ReplicatedLogs/${database}/${logId}`);
    let plan = readAgencyAtKey(`Plan/ReplicatedLogs/${database}/${logId}`);
    let current = readAgencyAtKey(`Current/ReplicatedLogs/${database}/${logId}`);
    return {target, plan, current};
};

const replicatedLogSetPlanParticipantsFlags = function (logId, participants) {
    writeAgencyAtKey(`Plan/ReplicatedLogs/${database}/${logId}/participantsConfig`, participants);
    global.ArangoAgency.increaseVersion(`Plan/Version`);
};

const replicatedLogUpdatePlanParticipantsFlags = function (logId, participants) {
    let oldValue = readAgencyAtKey(`Plan/ReplicatedLogs/${database}/${logId}/participantsConfig`);
    oldValue = oldValue || {generation: 0, participants: {}};
    for (const [p, v] of Object.entries(participants)) {
        if (v === null) {
            delete oldValue.participants[p];
        } else {
            oldValue.participants[p] = v;
        }
    }
    let gen = oldValue.generation += 1;
    replicatedLogSetPlanParticipantsFlags(logId, oldValue);
    return gen;
};

const replicatedLogSetPlanTerm = function (logId, term) {
    writeAgencyAtKey(`Plan/ReplicatedLogs/${database}/${logId}/currentTerm`, term);
    global.ArangoAgency.increaseVersion(`Plan/Version`);
};

const dbservers = (function () {
    return global.ArangoClusterInfo.getDBServers().map((x) => x.serverId);
}());

const getServerRebootId = function (serverId) {
    return readAgencyAtKey(`Current/ServersKnown/${serverId}/rebootId`);
};


const replicatedLogSuite = function () {

    /*
        1. Create replicated log:
            1.1. wait for supervision create a new term with leader.
            1.2. wait for leader established.
        2. Disable supervision intervention on Plan.
        3. Add or remove follower flags.
            3.1 wait for leader to confirm in current.
        4. Change leader
            4.1. wait for new leader to confirm in current.
     */

    const nextLogId = (function () {
        let logId = 100;
        return function () {
            return logId++;
        };
    }());

    const targetConfig = {
        writeConcern: 2,
        softWriteConcern: 2,
        replicationFactor: 3,
        waitForSync: false,
    };

    const waitFor = function (checkFn, maxTries = 100) {
        let count = 0;
        let result = null;
        while (count < maxTries) {
            result = checkFn();
            if (result === true) {
                return result;
            }
            assertTrue(result instanceof Error);
            print(result);
            count += 1;
            wait(0.5);
        }
        throw result;
    };

    const replicatedLogHasTermPlan = function (logId, termId = undefined) {
        return function () {
            let {plan} = readReplicatedLogAgency(logId);
            if (plan && plan.currentTerm) {
                return termId === undefined || (plan.currentTerm.term >= termId);
            }
            return false;
        };
    };

    const replicatedLogHasTermAndLeaderPlan = function (logId, leaderId = undefined) {
        return function () {
            let {plan} = readReplicatedLogAgency(logId);
            if (plan && plan.currentTerm && plan.currentTerm.leader) {
                return leaderId === undefined || (plan.currentTerm.leader.serverId === leaderId);
            }
            return false;
        };
    };

    const getParticipantsObjectForServers = function (servers) {
        return _.reduce(servers, (a, v) => {
            a[v] = {};
            return a;
        }, {});
    };

    const createTermSpecification = function (term, servers, config, leader) {
        let spec = {term, config};
        assertTrue(config.replicationFactor === servers.length);
        spec.participants = getParticipantsObjectForServers(servers);
        if (leader !== undefined) {
            assertTrue(_.includes(servers, leader));
            spec.leader = {serverId: leader, rebootId: getServerRebootId(leader)};
        }
        return spec;
    };

    const replicatedLogIsReady = function (logId, term, participants, leader) {
        return function () {
            let {current} = readReplicatedLogAgency(logId);
            if (current === undefined) {
                return Error("current not yet defined");
            }

            for (const srv of participants) {
                if (!current.localStatus || !current.localStatus[srv] || current.localStatus[srv].term !== term) {
                    return Error(`Participant ${srv} has not yet acknowledge the current term.`);
                }
            }

            if (leader !== undefined) {
                if (!current.leader || current.leader.term !== term || current.leader.serverId !== leader) {
                    return Error("Leader has not yet established its term");
                }
            }
            return true;
        };
    };

    const replicatedLogParticipantGeneration = function (logId, generation) {
        return function () {
            let {plan, current} = readReplicatedLogAgency(logId);
            if (current === undefined) {
                return Error("current not yet defined");
            }
            if (!current.leader) {
                return Error("Leader has not yet established its term");
            }
            if (!current.leader.committedParticipantsConfig || current.leader.committedParticipantsConfig.generation < generation) {
                return Error("Leader has not yet acked new generation");
            }
            return true;
        };
    };

    return {
        testCreateReplicatedLog: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const log = db._createReplicatedLog({
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers, leader));

            log.drop();
        },

        testAddParticipantFlag: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const log = db._createReplicatedLog({
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers, leader));

            // now update the excluded flag for one participant
            const follower = servers[1];
            let newGeneration = replicatedLogUpdatePlanParticipantsFlags(logId, {[follower]: {excluded: true}});
            waitFor(replicatedLogParticipantGeneration(logId, newGeneration));

            {
                let {current} = readReplicatedLogAgency(logId);
                assertTrue(current.leader.committedParticipantsConfig.participants[follower].excluded);
            }
            newGeneration = replicatedLogUpdatePlanParticipantsFlags(logId, {[follower]: null});
            waitFor(replicatedLogParticipantGeneration(logId, newGeneration));
            {
                let {current} = readReplicatedLogAgency(logId);
                assertEqual(current.leader.committedParticipantsConfig.participants[follower], undefined);
            }
            log.drop();
        },

        testUpdateTermInPlanLog: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const log = db._createReplicatedLog({
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers, leader));
            replicatedLogSetPlanTerm(logId, createTermSpecification(term + 1, servers, targetConfig, leader));

            // wait again for all servers to have acked term
            waitFor(replicatedLogIsReady(logId, term + 1, servers, leader));
            log.drop();
        },

        testUpdateTermInPlanLogWithNewLeader: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const log = db._createReplicatedLog({
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers));
            // wait again for all servers to have acked term
            const otherLeader = servers[1];
            replicatedLogSetPlanTerm(logId, createTermSpecification(term + 1, servers, targetConfig, otherLeader));
            waitFor(replicatedLogIsReady(logId, term + 1, servers, otherLeader));
            log.drop();
        },

    };
};

jsunity.run(replicatedLogSuite);
return jsunity.done();
