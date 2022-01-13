/*jshint strict: true */
/*global assertTrue, assertEqual*/
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
const {sleep} = require('internal');
const db = arangodb.db;
const ERRORS = arangodb.errors;
const helper = require("@arangodb/testutils/replicated-logs-helper");
const {
    waitFor,
    readReplicatedLogAgency,
    replicatedLogSetPlan,
    replicatedLogDeletePlan,
    createTermSpecification,
    replicatedLogIsReady,
    dbservers, allServersHealthy,
    nextUniqueLogId, interestingSetOfConfigurations, instantiateTestSuite,
    registerAgencyTestBegin, registerAgencyTestEnd,
} = helper;

const database = '_system'; // TODO change to something else

const replicatedLogLeaderElectionFailed = function (database, logId, term, servers) {
    return function () {
        let {current} = readReplicatedLogAgency(database, logId);
        if (current === undefined) {
            return Error("current not yet defined");
        }

        if (current.supervision === undefined || current.supervision.election === undefined) {
            return Error("supervision not yet updated");
        }

        let election = current.supervision.election;
        if (election.term !== term) {
            return Error("supervision report not yet available for current term; "
                + `found = ${election.term}; expected = ${term}`);
        }

        if (servers !== undefined) {
            // wait for all specified servers to have the proper error code
            for (let x of Object.keys(servers)) {
                if (election.details[x] === undefined) {
                    return Error(`server ${x} not in election result`);
                }
                let code = election.details[x].code;
                if (code !== servers[x]) {
                    return Error(`server ${x} reported with code ${code}, expected ${servers[x]}`);
                }
            }
        }

        return true;
    };
};

const replicatedLogLeaderElectionFailedNotOk = function (database, logId, term, servers) {
    const v = _.fromPairs(_.map(servers, (s) => [s, 1]));
    return replicatedLogLeaderElectionFailed(database, logId, term, v);
};

const commonHelperFunctions = (function () {
    let previousDatabase, databaseExisted = true;
    let stoppedServers = {};

    const continueServer = function (serverId) {
        if (stoppedServers[serverId] === undefined) {
            throw Error(`${serverId} not stopped`);
        }
        helper.continueServer(serverId);
        delete stoppedServers[serverId];
    };

    const stopServer = function (serverId) {
        if (stoppedServers[serverId] !== undefined) {
            throw Error(`${serverId} already stopped`);
        }
        helper.stopServer(serverId);
        stoppedServers[serverId] = true;
    };

    return {
        setUpAll: function () {
            previousDatabase = db._name();
            if (!_.includes(db._databases(), database)) {
                db._createDatabase(database);
                databaseExisted = false;
            }
            db._useDatabase(database);
        },
        stopServer,
        continueServer,

        tearDownAll: function () {
            Object.keys(stoppedServers).forEach(function (key) {
                continueServer(key);
            });

            db._useDatabase(previousDatabase);
            if (!databaseExisted) {
                db._dropDatabase(database);
            }
        },
        resumeAll: function () {
            Object.keys(stoppedServers).forEach(function (key) {
                helper.continueServer(key);
            });
        },
    };
}());

const requiredServersToContinueWorking = function (targetConfig) {
    return Math.max(targetConfig.writeConcern, targetConfig.replicationFactor - targetConfig.writeConcern + 1);
};

const withstandsNumberOfFailures = function (config, numberOfFailures) {
    return requiredServersToContinueWorking(config) <= config.replicationFactor - numberOfFailures;
};

const getDeclaredLeader = function (database, logId) {
    const {plan} = readReplicatedLogAgency(database, logId);
    if (plan.currentTerm && plan.currentTerm.leader) {
        return plan.currentTerm.leader.serverId;
    }
    return undefined;
};

const complexFailoverTestSuite = function (targetConfig) {
    const {setUpAll, tearDownAll, stopServer, continueServer} = commonHelperFunctions;

    const createReplicatedLog = function () {
        const logId = nextUniqueLogId();
        const servers = _.sampleSize(dbservers, targetConfig.replicationFactor);
        const leader = servers[0];
        const term = 1;
        replicatedLogSetPlan(database, logId, {
            id: logId,
            targetConfig,
            currentTerm: createTermSpecification(term, servers, targetConfig, leader),
        });
        const followers = _.difference(servers, [leader]);
        console.info(`Created replicated log ${logId} with leader ${leader} on servers ${servers}`);
        return {term, servers, leader, logId, followers};
    };

    return {
        setUpAll, tearDownAll,
        setUp: registerAgencyTestBegin,
        tearDown: function() {
            waitFor(allServersHealthy());
            registerAgencyTestEnd();
        },

        testCheckLeaderFail: function () {
            const expectFailover = withstandsNumberOfFailures(targetConfig, 1);
            const willElectLeader = targetConfig.replicationFactor - targetConfig.writeConcern + 1 <= targetConfig.replicationFactor - 1;
            const {term, leader, logId, followers, servers} = createReplicatedLog();

            waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
            stopServer(leader);

            if (expectFailover) {
                waitFor(replicatedLogIsReady(database, logId, term + 2, followers, followers));
                continueServer(leader);
            } else if (willElectLeader) {
                // leader will not establish
                waitFor(replicatedLogIsReady(database, logId, term + 2, followers));
                const newLeader = getDeclaredLeader(database, logId);
                assertTrue(newLeader !== undefined);
                continueServer(leader);
                waitFor(replicatedLogIsReady(database, logId, term + 2, servers, newLeader));
            } else {
                waitFor(replicatedLogLeaderElectionFailedNotOk(database, logId, term + 1, [leader]));
                continueServer(leader);
                waitFor(replicatedLogIsReady(database, logId, term + 2, servers, servers));
            }

            replicatedLogDeletePlan(database, logId);
        },
        // TODO add this test when commit fail reason is available even when leadership is not established
        // testCheckExcludedServer: function () {
        //     /*
        //         1. mark a set of servers as excluded. writeConcern many servers should remain non-excluded.
        //         2. stop the leader.
        //         4. wait for leader election
        //             4.1. if a leader was elected, it will not be able to establish its term.
        //                 4.1.1. wait for commit fail reason: excluded server
        //                 4.1.2. remove excluded flag from one of the servers
        //                 4.1.3. await leadership established
        //             4.2. if a leader can not be elected,
        //                 4.2.1. check that all excluded servers are reported as excluded
        //                 4.2.2. check that all servers report their term
        //                 4.2.3. remove excluded flag from one of the servers
        //                 4.2.3. await newly elected leader
        //      */
        //     const expectFailover = withstandsNumberOfFailures(targetConfig, 1);
        //     const willElectLeader = targetConfig.replicationFactor - targetConfig.writeConcern + 1 <= targetConfig.replicationFactor - 1;
        //     const {term, leader, logId, followers, servers} = createReplicatedLog();
        //
        //     waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
        //     stopServer(leader);
        //
        //     if (expectFailover) {
        //         waitFor(replicatedLogIsReady(database, logId, term + 2, followers, followers));
        //     } else if (willElectLeader) {
        //         // leader will not establish
        //         waitFor(replicatedLogIsReady(database, logId, term + 2, followers));
        //         const newLeader = getDeclaredLeader(database, logId);
        //         assertTrue(newLeader !== undefined);
        //         continueServer(leader);
        //         waitFor(replicatedLogIsReady(database, logId, term + 2, servers, newLeader));
        //     } else {
        //         waitFor(replicatedLogLeaderElectionFailedNotOk(database, logId, term + 1, [leader]));
        //         continueServer(leader);
        //         waitFor(replicatedLogIsReady(database, logId, term + 2, servers, servers));
        //     }
        //
        //     replicatedLogDeletePlan(database, logId);
        // },
    };
};

for (const config of interestingSetOfConfigurations) {
    jsunity.run(instantiateTestSuite(complexFailoverTestSuite, config));
}

return jsunity.done();
