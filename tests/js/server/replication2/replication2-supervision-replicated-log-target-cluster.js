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
    createParticipantsConfig,
    readReplicatedLogAgency,
    replicatedLogSetPlan,
    replicatedLogDeletePlan,
    createTermSpecification,
    replicatedLogIsReady,
    dbservers,
    nextUniqueLogId,
    registerAgencyTestBegin, registerAgencyTestEnd,
} = helper;

const database = '_system'; // TODO change to something else

const waitForReplicatedLogAvailable = function (id) {
    while (true) {
        try {
            let status = db._replicatedLog(id).status();
            if (status.role === "leader") {
                break;
            }
        } catch (err) {
            const errors = [
                ERRORS.ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED.code,
                ERRORS.ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND.code
            ];
            if (errors.indexOf(err.errorNum) === -1) {
                throw err;
            }
        }

        sleep(1);
    }
};

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

const replicatedLogSuite = function () {

    const targetConfig = {
        writeConcern: 2,
        softWriteConcern: 2,
        replicationFactor: 3,
        waitForSync: false,
    };

    const {setUpAll, tearDownAll, stopServer, continueServer} = (function () {
        let previousDatabase, databaseExisted = true;
        let stoppedServers = {};
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
                Object.keys(stoppedServers).forEach(function (key) {
                    continueServer(key);
                });

                db._useDatabase(previousDatabase);
                if (!databaseExisted) {
                    db._dropDatabase(database);
                }
            },
            stopServer: function (serverId) {
                if (stoppedServers[serverId] !== undefined) {
                    throw Error(`{serverId} already stopped`);
                }
                helper.stopServer(serverId);
                stoppedServers[serverId] = true;
            },
            continueServer: function (serverId) {
                if (stoppedServers[serverId] === undefined) {
                    throw Error(`{serverId} not stopped`);
                }
                helper.continueServer(serverId);
                delete stoppedServers[serverId];
            },
        };
    }());

    return {
        setUpAll, tearDownAll,
        setUp: registerAgencyTestBegin,
        tearDown: registerAgencyTestEnd,

        testCheckSimpleFailover: function () {
            const logId = nextUniqueLogId();
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
            waitFor(replicatedLogIsReady(database, logId, term, servers, leader));
            waitForReplicatedLogAvailable(logId);

            // now stop one server
            stopServer(servers[1]);

            // we should still be able to write
            {
                let log = db._replicatedLog(logId);
                // we have to insert two log entries here, reason:
                // Even though servers[1] is stopped, it will receive the AppendEntries message for log index 1.
                // It will stay in its tcp input queue. So when the server is continued below it will process
                // this message. However, the leader sees this message as still in flight and thus will never
                // send any updates again. By inserting yet another log entry, we can make sure that servers[2]
                // is the only server that has received log index 2.
                log.insert({foo: "bar"});
                let quorum = log.insert({foo: "bar"});
                assertTrue(quorum.result.quorum.quorum.indexOf(servers[1]) === -1);
            }

            // now stop the leader
            stopServer(leader);

            // since writeConcern is 2, there is no way a new leader can be elected
            waitFor(replicatedLogLeaderElectionFailed(database, logId, term + 1, {
                [leader]: 1,
                [servers[1]]: 1,
                [servers[2]]: 0,
            }));

            {
                // check election result
                const {current} = readReplicatedLogAgency(database, logId);
                const election = current.supervision.election;
                assertEqual(election.term, term + 1);
                assertEqual(election.participantsRequired, 2);
                assertEqual(election.participantsAvailable, 1);
                const detail = election.details;
                assertEqual(detail[leader].code, 1);
                assertEqual(detail[servers[1]].code, 1);
                assertEqual(detail[servers[2]].code, 0);
            }

            // now resume, servers[2] has to become leader, because it's the only server with log entry 1 available
            continueServer(servers[1]);
            waitFor(replicatedLogIsReady(database, logId, term + 2, [servers[1], servers[2]], servers[2]));

            replicatedLogDeletePlan(database, logId);
        },
    };
};

jsunity.run(replicatedLogSuite);
return jsunity.done();
