/*jshint strict: true */
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
const {
    waitFor,
    readReplicatedLogAgency,
    replicatedLogSetPlan,
    replicatedLogDeletePlan,
    replicatedLogUpdatePlanParticipantsConfigParticipants,
    replicatedLogSetPlanTerm,
    createParticipantsConfig,
    createTermSpecification,
    dbservers,
} = require("@arangodb/testutils/replicated-logs-helper");

const database = 'ReplLogsMaintenanceTest';

const replicatedLogIsReady = function (logId, term, participants, leader) {
    return function () {
        let {current} = readReplicatedLogAgency(database, logId);
        if (current === undefined) {
            return Error("current not yet defined");
        }

        for (const srv of participants) {
            if (!current.localStatus || !current.localStatus[srv]) {
                return Error(`Participant ${srv} has not yet reported to current.`);
            }
            if (current.localStatus[srv].term < term) {
                return Error(`Participant ${srv} has not yet acknowledged the current term; ` +
                    `found = ${current.localStatus[srv].term}, expected = ${term}.`);
            }
        }

        if (leader !== undefined) {
            if (!current.leader || current.leader.term < term || current.leader.serverId !== leader) {
                return Error("Leader has not yet established its term");
            }
        }
        return true;
    };
};

const replicatedLogParticipantGeneration = function (logId, generation) {
    return function () {
        let {current} = readReplicatedLogAgency(database, logId);
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

const replicatedLogParticipantsFlag = function (logId, flags, generation = undefined) {
    return function () {
        let {current} = readReplicatedLogAgency(database, logId);
        if (current === undefined) {
            return Error("current not yet defined");
        }
        if (!current.leader) {
            return Error("Leader has not yet established its term");
        }
        if (!current.leader.committedParticipantsConfig) {
            return Error("Leader has not yet committed any participants config");
        }
        if (generation !== undefined) {
            if (current.leader.committedParticipantsConfig.generation < generation) {
                return Error("Leader has not yet acked new generation; "
                    + `found ${current.leader.committedParticipantsConfig.generation}, expected = ${generation}`);
            }
        }

        const participants = current.leader.committedParticipantsConfig.participants;
        for (const [p, v] of Object.entries(flags)) {
            if (v === null) {
                if (participants[p] !== undefined) {
                    return Error(`Entry for server ${p} still present in participants flags`);
                }
            } else {
                if (!_.isEqual(v, participants[p])) {
                    return Error(`Flags for participant ${p} are not as expected: ${JSON.stringify(v)} vs. ${JSON.stringify(participants[p])}`);
                }
            }
        }

        return true;
    };
};

const replicatedLogSuite = function () {

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

    const {setUpAll, tearDownAll} = (function () {
        let previousDatabase, databaseExisted = true;
        return {
            setUpAll: function () {
                previousDatabase = db._name();
                if (!_.includes(db._databases(), database)) {
                    db._createDatabase(database);
                    databaseExisted = false;
                }
                db._useDatabase(database);
            },

            tearDownAll: function () {
                db._useDatabase(previousDatabase);
                if (!databaseExisted) {
                    db._dropDatabase(database);
                }
            }
        };
    }());

    return {
        setUpAll, tearDownAll,

        testCreateReplicatedLog: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const generation = 1;
            replicatedLogSetPlan(database, logId, {
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
                participantsConfig: createParticipantsConfig(generation, servers),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers, leader));

            replicatedLogDeletePlan(database, logId);
        },

        testCreateReplicatedLogWithoutLeader: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const term = 1;
            const generation = 1;
            replicatedLogSetPlan(database, logId, {
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig),
                participantsConfig: createParticipantsConfig(generation, servers),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers));

            replicatedLogDeletePlan(database, logId);
        },

        testAddParticipantFlag: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const generation = 1;
            const participantsConfig = createParticipantsConfig(generation, servers);
            replicatedLogSetPlan(database, logId, {
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
                participantsConfig,
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers, leader));

            // now update the excluded flag for one participant
            const follower = servers[1];
            let newGeneration = replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {[follower]: {excluded: true, forced: false}});
            waitFor(replicatedLogParticipantsFlag(logId, {[follower]: {excluded: true, forced: false}}, newGeneration));

            newGeneration = replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {[follower]: {excluded: false, forced: false}});
            waitFor(replicatedLogParticipantGeneration(logId, newGeneration));

            waitFor(replicatedLogParticipantsFlag(logId, {[follower]: {excluded: false, forced: false}}, newGeneration));
            replicatedLogDeletePlan(database, logId);
        },

        testUpdateTermInPlanLog: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const generation = 1;
            replicatedLogSetPlan(database, logId, {
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
                participantsConfig: createParticipantsConfig(generation, servers),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers, leader));
            replicatedLogSetPlanTerm(database, logId, createTermSpecification(term + 1, servers, targetConfig, leader));

            // wait again for all servers to have acked term
            waitFor(replicatedLogIsReady(logId, term + 1, servers, leader));
            replicatedLogDeletePlan(database, logId);
        },

        testUpdateTermInPlanLogWithNewLeader: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const term = 1;
            const generation = 1;
            replicatedLogSetPlan(database, logId, {
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
                participantsConfig: createParticipantsConfig(generation, servers),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers));
            // wait again for all servers to have acked term
            const otherLeader = servers[1];
            replicatedLogSetPlanTerm(database, logId, createTermSpecification(term + 1, servers, targetConfig, otherLeader));
            waitFor(replicatedLogIsReady(logId, term + 1, servers, otherLeader));
            replicatedLogDeletePlan(database, logId);
        },

        testAddParticipant: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const leader = servers[0];
            const remaining = _.difference(dbservers, servers);
            const term = 1;
            const generation = 1;
            replicatedLogSetPlan(database, logId, {
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, servers, targetConfig, leader),
                participantsConfig: createParticipantsConfig(generation, servers),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, servers));
            // now add an additional participant to the participants configuration
            const newServer = _.sample(remaining);
            const newServers = [...servers, newServer];
            replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
                [newServer]: {
                    excluded: false,
                    forced: false
                }
            });
            waitFor(replicatedLogIsReady(logId, term, newServers, leader));
            replicatedLogDeletePlan(database, logId);
        },

        testRemoveParticipant: function () {
            const logId = nextLogId();
            const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
            const remaining = _.difference(dbservers, servers);
            const toBeRemoved = _.sample(remaining);
            const leader = servers[0];
            const term = 1;
            const generation = 1;
            const newServers = [...servers, toBeRemoved];
            replicatedLogSetPlan(database, logId, {
                id: logId,
                targetConfig,
                currentTerm: createTermSpecification(term, newServers, targetConfig, leader),
                participantsConfig: createParticipantsConfig(generation, newServers),
            });

            // wait for all servers to have reported in current
            waitFor(replicatedLogIsReady(logId, term, newServers));
            replicatedLogUpdatePlanParticipantsConfigParticipants(database, logId, {
                [toBeRemoved]: null
            });
            waitFor(replicatedLogParticipantsFlag(logId, {[toBeRemoved]: null}));
            replicatedLogDeletePlan(database, logId);
        },
    };
};

jsunity.run(replicatedLogSuite);
return jsunity.done();
