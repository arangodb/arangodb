/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotMatch, assertNotEqual, assertNotUndefined, assertMatch */

// //////////////////////////////////////////////////////////////////////////////
// / @brief 
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / @author 
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");


let api = "/_api/replication";

function dealing_with_general_function_interfaceSuite () {
  return {

    test_fetches_the_server_id_1: function() {
      // fetch id;
      let cmd = api + "/server-id";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertMatch(/^[0-9]+$/, doc.parsedBody['serverId']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// applier;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_applier_interfaceSuite () {
  return {

    setUp: function() {
      arango.PUT_RAW(api + "/applier-stop", "");
      arango.DELETE_RAW(api + "/applier-state", "");
    },

    tearDown: function() {
      arango.PUT_RAW(api + "/applier-stop", "");
      arango.DELETE_RAW(api + "/applier-state", "");
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // start;
    ////////////////////////////////////////////////////////////////////////////////;

    test_starts_the_applier_1: function() {
      let cmd = api + "/applier-start";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 400); // because configuration is invalid
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // stop;
    ////////////////////////////////////////////////////////////////////////////////;

    test_stops_the_applier_1: function() {
      let cmd = api + "/applier-stop";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // properties;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_applier_config_1: function() {
      let cmd = api + "/applier-config";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertEqual(typeof all["requestTimeout"], 'number');
      assertEqual(typeof all["connectTimeout"], 'number');
      assertEqual(typeof all["ignoreErrors"], 'number');
      assertEqual(typeof all["maxConnectRetries"], 'number');
      assertEqual(typeof all["sslProtocol"], 'number');
      assertEqual(typeof all["chunkSize"], 'number');
      assertTrue(all.hasOwnProperty("autoStart"));
      assertTrue(all.hasOwnProperty("adaptivePolling"));
      assertTrue(all.hasOwnProperty("autoResync"));
      assertTrue(all.hasOwnProperty("includeSystem"));
      assertTrue(all.hasOwnProperty("requireFromPresent"));
      assertTrue(all.hasOwnProperty("verbose"));
      assertEqual(typeof all["restrictType"], 'string');
      assertEqual(typeof all["connectionRetryWaitTime"], 'number');
      assertEqual(typeof all["initialSyncMaxWaitTime"], 'number');
      assertEqual(typeof all["idleMinWaitTime"], 'number');
      assertEqual(typeof all["idleMaxWaitTime"], 'number');
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // set && fetch properties;
    ////////////////////////////////////////////////////////////////////////////////;

    test_sets_and_re_fetches_the_applier_config_1: function() {
      let cmd = api + "/applier-config";
      let body = { "endpoint" : "tcp://127.0.0.1:9999", "database" : "foo", "ignoreErrors" : 5, "requestTimeout" : 32.2, "connectTimeout" : 51.1, "maxConnectRetries" : 12345, "chunkSize" : 143423232, "autoStart" : true, "adaptivePolling" : false, "autoResync" : true, "includeSystem" : true, "requireFromPresent" : true, "verbose" : true, "connectionRetryWaitTime" : 22.12, "initialSyncMaxWaitTime" : 12.21, "idleMinWaitTime" : 1.4, "idleMaxWaitTime" : 7.3 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertEqual(all["endpoint"], "tcp://127.0.0.1:9999");
      assertEqual(all["database"], "foo");
      assertEqual(all["requestTimeout"], 32.2);
      assertEqual(all["connectTimeout"], 51.1);
      assertEqual(all["ignoreErrors"], 5);
      assertEqual(all["maxConnectRetries"], 12345);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 143423232);
      assertTrue(all["autoStart"]);
      assertFalse(all["adaptivePolling"]);
      assertTrue(all["autoResync"]);
      assertTrue(all["includeSystem"]);
      assertTrue(all["requireFromPresent"]);
      assertTrue(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 22.12);
      assertEqual(all["initialSyncMaxWaitTime"], 12.21);
      assertEqual(all["idleMinWaitTime"], 1.4);
      assertEqual(all["idleMaxWaitTime"], 7.3);

      // refetch same data;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      all = doc.parsedBody;
      assertEqual(all["endpoint"], "tcp://127.0.0.1:9999");
      assertEqual(all["database"], "foo");
      assertEqual(all["requestTimeout"], 32.2);
      assertEqual(all["connectTimeout"], 51.1);
      assertEqual(all["ignoreErrors"], 5);
      assertEqual(all["maxConnectRetries"], 12345);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 143423232);
      assertTrue(all["autoStart"]);
      assertFalse(all["adaptivePolling"]);
      assertTrue(all["autoResync"]);
      assertTrue(all["includeSystem"]);
      assertTrue(all["requireFromPresent"]);
      assertTrue(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 22.12);
      assertEqual(all["initialSyncMaxWaitTime"], 12.21);
      assertEqual(all["idleMinWaitTime"], 1.4);
      assertEqual(all["idleMaxWaitTime"], 7.3);


      body = { "endpoint" : "ssl://127.0.0.1:12345", "database" : "bar", "ignoreErrors" : 2, "requestTimeout" : 12.5, "connectTimeout" : 26.3, "maxConnectRetries" : 12, "chunkSize" : 1234567, "autoStart" : false, "adaptivePolling" : true, "autoResync" : false, "includeSystem" : false, "requireFromPresent" : false, "verbose" : false, "connectionRetryWaitTime" : 2.5, "initialSyncMaxWaitTime" : 4.3, "idleMinWaitTime" : 0.22, "idleMaxWaitTime" : 3.5 };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      all = doc.parsedBody;
      assertEqual(all["endpoint"], "ssl://127.0.0.1:12345");
      assertEqual(all["database"], "bar");
      assertEqual(all["requestTimeout"], 12.5);
      assertEqual(all["connectTimeout"], 26.3);
      assertEqual(all["ignoreErrors"], 2);
      assertEqual(all["maxConnectRetries"], 12);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 1234567);
      assertFalse(all["autoStart"]);
      assertTrue(all["adaptivePolling"]);
      assertFalse(all["autoResync"]);
      assertFalse(all["includeSystem"]);
      assertFalse(all["requireFromPresent"]);
      assertFalse(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 2.5);
      assertEqual(all["initialSyncMaxWaitTime"], 4.3);
      assertEqual(all["idleMinWaitTime"], 0.22);
      assertEqual(all["idleMaxWaitTime"], 3.5);

      // refetch same data;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      all = doc.parsedBody;
      assertEqual(all["endpoint"], "ssl://127.0.0.1:12345");
      assertEqual(all["database"], "bar");
      assertEqual(all["requestTimeout"], 12.5);
      assertEqual(all["connectTimeout"], 26.3);
      assertEqual(all["ignoreErrors"], 2);
      assertEqual(all["maxConnectRetries"], 12);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 1234567);
      assertFalse(all["autoStart"]);
      assertTrue(all["adaptivePolling"]);
      assertFalse(all["autoResync"]);
      assertFalse(all["includeSystem"]);
      assertFalse(all["requireFromPresent"]);
      assertFalse(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 2.5);
      assertEqual(all["initialSyncMaxWaitTime"], 4.3);
      assertEqual(all["idleMinWaitTime"], 0.22);
      assertEqual(all["idleMaxWaitTime"], 3.5);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // state;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_applier_state_1: function() {
      // fetch state;
      let cmd = api + "/applier-state";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('state'));
      assertTrue(all.hasOwnProperty('server'));

      let state = all['state'];
      assertFalse(state['running']);
      assertTrue(state.hasOwnProperty("lastAppliedContinuousTick"));
      assertTrue(state.hasOwnProperty("lastProcessedContinuousTick"));
      assertTrue(state.hasOwnProperty("lastAvailableContinuousTick"));
      assertTrue(state.hasOwnProperty("safeResumeTick"));

      assertTrue(state.hasOwnProperty("progress"));
      let progress = state['progress'];
      assertTrue(progress.hasOwnProperty("time"));
      assertMatch(/^([0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z)?$/, progress['time']);
      assertTrue(progress.hasOwnProperty("failedConnects"));

      assertTrue(state.hasOwnProperty("totalRequests"));
      assertTrue(state.hasOwnProperty("totalFailedConnects"));
      assertTrue(state.hasOwnProperty("totalEvents"));
      assertTrue(state.hasOwnProperty("totalOperationsExcluded"));

      assertTrue(state.hasOwnProperty("lastError"));
      let lastError = state["lastError"];
      assertTrue(lastError.hasOwnProperty("errorNum"));
      assertTrue(state.hasOwnProperty("time"));
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// logger;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_loggerSuite () {
  return {
    setUp: function() {
      db._drop("UnitTestsReplication");
    },

    tearDown: function() {
      db._drop("UnitTestsReplication");
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // state;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_logger_state: function() {
      // fetch state;
      let cmd = api + "/logger-state";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('state'));
      assertTrue(all.hasOwnProperty('server'));
      assertTrue(all.hasOwnProperty('clients'));

      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      let server = all['server'];
      assertEqual(server['engine'], "rocksdb");
      assertMatch(/^[0-9]+$/, server['serverId']);
      assertTrue(server.hasOwnProperty('version'));
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // firstTick;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_first_available_tick: function() {
      // fetch state;
      let cmd = api + "/logger-first-tick";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      let result = doc.parsedBody;
      assertTrue(result.hasOwnProperty('firstTick'));

      assertMatch(/^[0-9]+$/, result['firstTick']);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // tickRanges;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_available_tick_ranges: function() {
      // fetch state;
      let cmd = api + "/logger-tick-ranges";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      let result = doc.parsedBody;
      assertTrue(doc.body.length > 0, doc);

      result.forEach(datafile => {
        assertTrue(datafile.hasOwnProperty('datafile'));
        assertTrue(datafile.hasOwnProperty('status'));
        assertTrue(datafile.hasOwnProperty('tickMin'));
        assertTrue(datafile.hasOwnProperty('tickMax'));
        assertMatch(/^[0-9]+$/, datafile['tickMin']);
        assertMatch(/^[0-9]+$/, datafile['tickMax']);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // follow;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_empty_follow_log_1: function() {
      while (true) {
        let cmd = api + "/logger-state";
        let doc = arango.GET_RAW(cmd);
        assertEqual(doc.code, 200);
        assertTrue(doc.parsedBody["state"]["running"]);
        let fromTick = doc.parsedBody["state"]["lastLogTick"];

        cmd = api + "/logger-follow?from=" + fromTick;
        doc = arango.GET_RAW(cmd);

        if (doc.code !== 204) {
          // someone else did something else;
          assertEqual(doc.code, 200);
          // sleep for a second && try again;
          sleep(1);
        } else {
          assertEqual(doc.code, 204);
          //assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
          assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
          assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
          assertEqual(doc.headers["content-type"], "application/x-arango-dump");

          let body = doc.body;
          assertEqual(body, undefined);
          break;
        }
      }
    },

    test_fetches_a_create_collection_action_from_the_follow_log_1: function() {
      db._drop("UnitTestsReplication");

      sleep(5);

      let cmd = api + "/logger-state";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["state"]["running"]);
      let fromTick = doc.parsedBody["state"]["lastLogTick"];

      let cid = db._create("UnitTestsReplication", { waitForSync: true });

      sleep(5);

      cmd = api + "/logger-follow?from=" + fromTick;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");
      
      let body = doc.body.toString();

      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }
        let part = body.slice(0, position);
        let document = JSON.parse(part);

        if (document["type"] === 2000 && document["cid"] === cid._id) {
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));
          assertTrue(document.hasOwnProperty("cname"));
          assertTrue(document.hasOwnProperty("data"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2000);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');
          assertEqual(document["cname"], "UnitTestsReplication");

          let c = document["data"];
          assertTrue(c.hasOwnProperty("version"));
          assertEqual(c["type"], 2);
          assertEqual(c["cid"], cid._id);
          assertEqual(typeof c["cid"], 'string');
          assertFalse(c["deleted"]);
          assertEqual(c["name"], "UnitTestsReplication");
          assertTrue(c["waitForSync"]);
        }

        body = body.slice(position + 1, body.length);
      }

    },

    test_fetches_some_collection_operations_from_the_follow_log_1: function() {
      db._drop("UnitTestsReplication");

      sleep(5);

      let cmd = api + "/logger-state";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["state"]["running"]);
      let fromTick = doc.parsedBody["state"]["lastLogTick"];

      // create collection;
      let cid = db._create("UnitTestsReplication", { waitForSync: true});

      // create document;
      cmd = "/_api/document?collection=UnitTestsReplication";
      let body = { "_key" : "test", "test" : false };
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      let rev = doc.parsedBody["_rev"];

      // delete document;
      cmd = "/_api/document/UnitTestsReplication/test";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);

      // drop collection;
      cmd = "/_api/collection/UnitTestsReplication";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);

      sleep(5);

      cmd = api + "/logger-follow?from=" + fromTick;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");


      body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);
        let document = JSON.parse(part);

        if (i === 0) {
          if (document["type"] === 2000 && document["cid"] === cid._id) {
            // create collection;
            assertTrue(document.hasOwnProperty("tick"));
            assertTrue(document.hasOwnProperty("type"));
            assertTrue(document.hasOwnProperty("cid"));
            assertTrue(document.hasOwnProperty("cname"));
            assertTrue(document.hasOwnProperty("data"));

            assertMatch(/^[0-9]+$/, document["tick"]);
            assertTrue(Number(document["tick"]) >= Number(fromTick));
            assertEqual(document["type"], 2000);
            assertEqual(document["cid"], cid._id);
            assertEqual(typeof document["cid"], 'string');
            assertEqual(document["cname"], "UnitTestsReplication");

            let c = document["data"];
            assertTrue(c.hasOwnProperty("version"));
            assertEqual(c["type"], 2);
            assertEqual(c["cid"], cid._id);
            assertEqual(typeof c["cid"], 'string');
            assertFalse(c["deleted"]);
            assertEqual(c["name"], "UnitTestsReplication");
            assertTrue(c["waitForSync"]);
          }
          i = i + 1;
        } else if ( i === 1 && document["type"] === 2300 && document["cid"] === cid._id) {
          // create document;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2300);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');
          assertEqual(document["data"]["_key"], "test");
          assertMatch(/^[a-zA-Z0-9_\-]+$/, document["data"]["_rev"]);
          assertNotEqual(document["data"]["_rev"], "0");
          assertFalse(document["data"]["test"]);

          i = i + 1;
        } else if ( i === 2 && document["type"] === 2302 && document["cid"] === cid._id) {
          // delete document;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2302);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');
          assertEqual(document["data"]["_key"], "test");

          i = i + 1;
        } else if ( i === 3 && document["type"] === 2001 && document["cid"] === cid._id) {
          // drop collection;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2001);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');

          i = i + 1;
        }

        body = body.slice(position + 1, body.length);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// batches && logger state requests;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_batches_and_logger_stateSuite () {
  let batchId=0;
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
    },

    test_creates_a_batch__without_state_request: function() {
      let doc = arango.POST_RAW(api + "/batch", "{}");
      assertEqual(doc.code, 200);
      batchId = doc.parsedBody['id'];
      assertMatch(/^[0-9]+$/, batchId);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('id'));
      assertMatch(/^[0-9]+$/, all['id']);
      assertTrue(all.hasOwnProperty('lastTick'));
      assertMatch(/^[0-9]+$/, all['lastTick']);

      assertFalse(all.hasOwnProperty('state'));
    },

    test_creates_a_batch__with_state_request: function() {
      let doc = arango.POST_RAW(api + "/batch?state=true", "{}");
      assertEqual(doc.code, 200);
      batchId = doc.parsedBody['id'];
      assertMatch(/^[0-9]+$/, batchId);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('id'));
      assertTrue(all.hasOwnProperty('lastTick'));
      assertMatch(/^[0-9]+$/, all['id']);
      assertMatch(/^[0-9]+$/, all['lastTick']);

      assertTrue(all.hasOwnProperty('state'));
      let state = all['state'];
      assertTrue(state.hasOwnProperty('state'));
      assertTrue(state['state'].hasOwnProperty('running'));
      assertTrue(state['state']['running']);
      assertTrue(state['state'].hasOwnProperty('lastLogTick'));
      assertMatch(/^[0-9]+$/, state['state']['lastLogTick']);
      assertTrue(state['state'].hasOwnProperty('lastUncommittedLogTick'));
      assertMatch(/^[0-9]+$/, state['state']['lastUncommittedLogTick']);
      assertEqual(state['state']['lastLogTick'], state['state']['lastUncommittedLogTick']);
      assertTrue(state['state'].hasOwnProperty('totalEvents'));
      assertTrue(state['state'].hasOwnProperty('time'));

      // make sure that all tick values in the response are equal;
      assertEqual(all['lastTick'], state['state']['lastLogTick']);

      assertTrue(state.hasOwnProperty('server'));
      assertTrue(state['server'].hasOwnProperty('version'));
      assertTrue(state['server'].hasOwnProperty('engine'));
      assertEqual(state['server']['engine'], 'rocksdb');
      assertTrue(state['server'].hasOwnProperty('serverId'));

      assertTrue(state.hasOwnProperty('clients'));
      assertEqual(state['clients'].length, 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// inventory / dump;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_initial_dump_interfaceSuite () {
  let batchId;
  return {
    setUp: function() {
      db._drop("UnitTestsReplication");
      db._drop("UnitTestsReplication2");
      let doc = arango.POST_RAW(api + "/batch", {});
      assertEqual(doc.code, 200);
      batchId = doc.parsedBody['id'];
      assertMatch(/^[0-9]+$/, batchId);
    },

    tearDown: function() {
      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      db._drop("UnitTestsReplication");
      db._drop("UnitTestsReplication2");
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // inventory;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_initial_inventory_1: function() {
      let cmd = api + `/inventory?includeSystem=false&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let collections = all["collections"];
      let filtered = [ ];
      collections.forEach( collection => {
        if ([ "UnitTestsReplication", "UnitTestsReplication2" ].find( name => {
          return name === collection["parameters"]["name"];
        }) !== undefined) {
          filtered.push(collection);
        }
      });
      assertEqual(filtered, [ ]);
      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);
    },

    test_checks_the_initial_inventory_for_non_system_collections_1: function() {
      let cmd = api + `/inventory?includeSystem=false&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let collections = all["collections"];
      collections.forEach(collection => {
        assertNotMatch(/^_/, collection["parameters"]["name"]);
      });
    },

    test_checks_the_initial_inventory_for_system_collections: function() {
      let cmd = api + `/inventory?includeSystem=true&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let collections = all["collections"];
      let systemCollections = 0;
      collections.forEach (collection => {
        if (collection["parameters"]["name"].match(/^_/)) {
          systemCollections = systemCollections + 1;
        }
      });
      assertNotEqual(systemCollections, 0);
    },

    test_checks_the_inventory_after_creating_collections_1: function() {
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._createEdgeCollection("UnitTestsReplication2", {waitForSync: true });

      let cmd = api + `/inventory?includeSystem=false&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      let collections = all['collections'];
      let filtered = [ ];
      collections.forEach (collection => {
        if ([ "UnitTestsReplication", "UnitTestsReplication2" ].find( name => {
          return name === collection["parameters"]["name"];
        }) !== undefined) {
          filtered.push(collection);
        }
      });
      assertEqual(filtered.length, 2);

      // first collection;
      let c = filtered[0];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      let parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 2);
      assertEqual(parameters["cid"], cid._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication");
      assertFalse(parameters["waitForSync"]);

      assertEqual(c['indexes'], [ ]);

      // second collection;
      c = filtered[1];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 3);
      assertEqual(parameters["cid"], cid2._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication2");
      assertTrue(parameters["waitForSync"]);

      assertEqual(c['indexes'], [ ]);
    },

    test_checks_the_inventory_with_indexes_1: function() {
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._create("UnitTestsReplication2");

      let body = { "type" : "hash", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW("/_api/index?collection=UnitTestsReplication", body);
      assertEqual(doc.code, 201);

      body = { "type" : "skiplist", "unique" : false, "fields" : [ "c" ] };
      doc = arango.POST_RAW("/_api/index?collection=UnitTestsReplication", body);
      assertEqual(doc.code, 201);

      // create indexes for second collection;
      body = { "type" : "skiplist", "unique" : true, "fields" : [ "d" ] };
      doc = arango.POST_RAW("/_api/index?collection=UnitTestsReplication2", body);
      assertEqual(doc.code, 201);

      let cmd = api + `/inventory?batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      let collections = all['collections'];
      let filtered = [ ];
      collections.forEach (collection => {
        if ([ "UnitTestsReplication", "UnitTestsReplication2" ].find( name => {
          return name === collection["parameters"]["name"];
        }) !== undefined) {
          filtered.push(collection);
        }
      });
      assertEqual(filtered.length, 2);

      // first collection;
      let c = filtered[0];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      let parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 2);
      assertEqual(parameters["cid"], cid._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication");
      assertFalse(parameters["waitForSync"]);

      let indexes = c['indexes'];
      assertEqual(indexes.length, 2);

      let idx = indexes[0];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "hash");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "a", "b" ]);

      idx = indexes[1];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "skiplist");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "c" ]);

      // second collection;
      c = filtered[1];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 2);
      assertEqual(parameters["cid"], cid2._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication2");
      assertFalse(parameters["waitForSync"]);

      indexes = c['indexes'];
      assertEqual(indexes.length, 1);

      idx = indexes[0];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "skiplist");
      assertTrue(idx["unique"]);
      assertEqual(idx["fields"], [ "d" ]);

    },

    ////////////////////////////////////////////////////////////////////////////////;
    // dump;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_dump_for_an_empty_collection_1: function() {
      let cid = db._create("UnitTestsReplication");

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 204);
      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertEqual(doc.headers["x-arango-replication-lastincluded"], "0", doc);
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");
      assertEqual(doc.body, undefined);
    },

    test_checks_the_dump_for_a_non_empty_collection_1: function() {
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 100; i++) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      };

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0", doc);
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let doc = JSON.parse(part);
        assertTrue(doc.hasOwnProperty("type"));
        assertTrue(doc.hasOwnProperty("data"));
        assertFalse(doc.hasOwnProperty("_key"));
        assertEqual(doc['type'], 2300);
        assertMatch(/^test[0-9]+$/, doc['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, doc["data"]["_rev"]);
        assertTrue(doc['data'].hasOwnProperty("test"));

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }

      assertEqual(i, 100);
    },

    test_checks_the_dump_for_a_non_empty_collection__no_envelopes: function() {
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 100; i++) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      }

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}&useEnvelope=false`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let doc = JSON.parse(part);
        assertFalse(doc.hasOwnProperty("type"));
        assertFalse(doc.hasOwnProperty("data"));
        assertTrue(doc.hasOwnProperty("_key"));
        assertMatch(/^test[0-9]+$/, doc['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, doc["_rev"]);
        assertTrue(doc.hasOwnProperty("test"));

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }

      assertEqual(i, 100);
    },

    test_checks_the_dump_for_a_non_empty_collection__small_chunkSize_1: function() {
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 100; i++) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      };

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", {});
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&chunkSize=1024&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "true");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let doc = JSON.parse(part);
        assertEqual(doc['type'], 2300);
        assertMatch(/^test[0-9]+$/, doc['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, doc["data"]["_rev"]);
        assertTrue(doc['data'].hasOwnProperty('test'));

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertTrue(i< 100);
    },


    test_checks_the_dump_for_an_edge_collection_1: function() {
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._createEdgeCollection("UnitTestsReplication2");

      for (let i = 0; i < 100; i++) {
        let body = { "_key" : `test${i}`, "_from" : "UnitTestsReplication/foo", "_to" : "UnitTestsReplication/bar", "test1" : `${i}`, "test2" : false, "test3" : [ ], "test4" : { } };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication2", body);
        assertEqual(doc.code, 202);
      }

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication2&chunkSize=65536&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let document = JSON.parse(part);
        assertEqual(document['type'], 2300);
        assertMatch(/^test[0-9]+$/, document['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, document["data"]["_rev"]);
        assertEqual(document['data']['_from'], "UnitTestsReplication/foo");
        assertEqual(document['data']['_to'], "UnitTestsReplication/bar");
        assertTrue(document['data'].hasOwnProperty('test1'));
        assertFalse(document['data']['test2']);
        assertEqual(document['data']['test3'], [ ]);
        assertEqual(document['data']['test4'], { });

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertEqual(i, 100);
    },

    test_checks_the_dump_for_an_edge_collection__small_chunkSize_1: function() {
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._createEdgeCollection("UnitTestsReplication2");

      for (let i = 0; i < 100; i ++) {
        let body = { "_key" : `test${i}`, "_from" : "UnitTestsReplication/foo", "_to" : "UnitTestsReplication/bar", "test1" : i, "test2" : false, "test3" : [ ], "test4" : { } };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication2", body);
        assertEqual(doc.code, 202);
      }

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication2&chunkSize=1024&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "true");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let document = JSON.parse(part);
        assertEqual(document['type'], 2300);
        assertMatch(/^test[0-9]+$/, document['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, document['data']['_rev']);
        assertEqual(document['data']['_from'], "UnitTestsReplication/foo");
        assertEqual(document['data']['_to'], "UnitTestsReplication/bar");
        assertTrue(document['data'].hasOwnProperty('test1'));
        assertFalse(document['data']['test2']);
        assertEqual(document['data']['test3'], [ ]);
        assertEqual(document['data']['test4'], { });

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertTrue(i < 100);
    },

    test_checks_the_dump_for_a_collection_with_deleted_documents_1: function() {
      let cid = db._create("UnitTestsReplication");

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      for (let i = 0; i < 100; i++) {
        let body = { "_key" : `test${i}`, "test" : `${i}`};
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);

        doc = arango.DELETE_RAW(`/_api/document/UnitTestsReplication/test${i}`, body);
        assertEqual(doc.code, 202);
      }

      doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 204);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");
    },

    test_checks_the_dump_for_a_truncated_collection_1: function() {
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 10; i++) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      }

      // truncate;
      let cmd = "/_api/collection/UnitTestsReplication/truncate";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);

      doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 204);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");
    },

    test_checks_the_dump_for_a_non_empty_collection__3_0_mode_1: function() {
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 100; i++) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      }

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);
        let doc = JSON.parse(part);
        assertEqual(doc['type'], 2300);
        assertFalse(doc.hasOwnProperty("key"));
        assertFalse(doc.hasOwnProperty("rev"));
        assertMatch(/^test[0-9]+$/, doc['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, doc['data']['_rev']);
        assertTrue(doc['data'].hasOwnProperty('test'));

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertEqual(i, 100);
    },

    test_fetches_incremental_parts_of_a_collection_dump_1: function() {
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 10; i++) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      }

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      let fromTick = 0;

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", {});
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      for (let i = 0; i < 10; i++) {
        let cmd = api + `/dump?collection=UnitTestsReplication&from=${fromTick}&chunkSize=1&batchId=${batchId}`;
        doc = arango.GET_RAW(cmd);
        assertEqual(doc.code, 200);

        if (i === 9) {
          assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
        } else {
          assertEqual(doc.headers["x-arango-replication-checkmore"], "true");
        }

        assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
        assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
        assertTrue(Number(doc.headers["x-arango-replication-lastincluded"]) >= Number(fromTick));
        assertEqual(doc.headers["content-type"], "application/x-arango-dump");

        fromTick = doc.headers["x-arango-replication-lastincluded"];

        let document = JSON.parse(doc.body.toString());
        assertEqual(document['type'], 2300);
        assertMatch(/^test[0-9]+$/, document['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, document['data']['_rev'], document);
        assertTrue(document['data'].hasOwnProperty('test'));
      }
    }
  };
}

function dealing_with_general_functionSuite () {
  let api = "/_db/UnitTestDB/_api/replication";
  return {
    setUp: function() {
      let res = db._createDatabase("UnitTestDB");;
      assertTrue(res);
    },

    tearDown: function() {
      let res = db._dropDatabase("UnitTestDB");;
      assertTrue(res);
    },
    test_fetches_the_server_id_2: function() {
      // fetch id;
      let cmd = api + "/server-id";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertMatch(/^[0-9]+$/, doc.parsedBody['serverId']);
    }
  };
}
////////////////////////////////////////////////////////////////////////////////;
// applier;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_applierSuite () {
  let api = "/_db/UnitTestDB/_api/replication";
  return {
    setUp: function() {
      let res = db._createDatabase("UnitTestDB");;
      assertTrue(res);
      arango.PUT_RAW(api + "/applier-stop", "");
      arango.DELETE_RAW(api + "/applier-state", "");
    },

    tearDown: function() {
      let res = db._dropDatabase("UnitTestDB");;
      assertTrue(res);
      arango.PUT_RAW(api + "/applier-stop", "");
      arango.DELETE_RAW(api + "/applier-state", "");
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // start;
    ////////////////////////////////////////////////////////////////////////////////;

    test_starts_the_applier_2: function() {
      let cmd = api + "/applier-start";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 400);// because configuration is invalid
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // stop;
    ////////////////////////////////////////////////////////////////////////////////;

    test_stops_the_applier_2: function() {
      let cmd = api + "/applier-stop";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // properties;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_applier_config_2: function() {
      let cmd = api + "/applier-config";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertEqual(typeof all["requestTimeout"], 'number');
      assertEqual(typeof all["connectTimeout"], 'number');
      assertEqual(typeof all["ignoreErrors"], 'number');
      assertEqual(typeof all["maxConnectRetries"], 'number');
      assertEqual(typeof all["sslProtocol"], 'number');
      assertEqual(typeof all["chunkSize"], 'number');
      assertTrue(all.hasOwnProperty("autoStart"));
      assertTrue(all.hasOwnProperty("adaptivePolling"));
      assertTrue(all.hasOwnProperty("autoResync"));
      assertTrue(all.hasOwnProperty("includeSystem"));
      assertTrue(all.hasOwnProperty("requireFromPresent"));
      assertTrue(all.hasOwnProperty("verbose"));
      assertEqual(typeof all["restrictType"], 'string');
      assertEqual(typeof all["connectionRetryWaitTime"], 'number');
      assertEqual(typeof all["initialSyncMaxWaitTime"], 'number');
      assertEqual(typeof all["idleMinWaitTime"], 'number');
      assertEqual(typeof all["idleMaxWaitTime"], 'number');
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // set && fetch properties;
    ////////////////////////////////////////////////////////////////////////////////;

    test_sets_and_re_fetches_the_applier_config_2: function() {
      let cmd = api + "/applier-config";
      let body = { "endpoint" : "tcp://127.0.0.1:9999", "database" : "foo", "ignoreErrors" : 5, "requestTimeout" : 32.2, "connectTimeout" : 51.1, "maxConnectRetries" : 12345, "chunkSize" : 143423232, "autoStart" : true, "adaptivePolling" : false, "autoResync" : true, "includeSystem" : true, "requireFromPresent" : true, "verbose" : true, "connectionRetryWaitTime" : 22.12, "initialSyncMaxWaitTime" : 12.21, "idleMinWaitTime" : 1.4, "idleMaxWaitTime" : 7.3 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertEqual(all["endpoint"], "tcp://127.0.0.1:9999");
      assertEqual(all["database"], "foo");
      assertEqual(all["requestTimeout"], 32.2);
      assertEqual(all["connectTimeout"], 51.1);
      assertEqual(all["ignoreErrors"], 5);
      assertEqual(all["maxConnectRetries"], 12345);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 143423232);
      assertTrue(all["autoStart"]);
      assertFalse(all["adaptivePolling"]);
      assertTrue(all["autoResync"]);
      assertTrue(all["includeSystem"]);
      assertTrue(all["requireFromPresent"]);
      assertTrue(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 22.12);
      assertEqual(all["initialSyncMaxWaitTime"], 12.21);
      assertEqual(all["idleMinWaitTime"], 1.4);
      assertEqual(all["idleMaxWaitTime"], 7.3);

      // refetch same data;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      all = doc.parsedBody;
      assertEqual(all["endpoint"], "tcp://127.0.0.1:9999");
      assertEqual(all["database"], "foo");
      assertEqual(all["requestTimeout"], 32.2);
      assertEqual(all["connectTimeout"], 51.1);
      assertEqual(all["ignoreErrors"], 5);
      assertEqual(all["maxConnectRetries"], 12345);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 143423232);
      assertTrue(all["autoStart"]);
      assertFalse(all["adaptivePolling"]);
      assertTrue(all["autoResync"]);
      assertTrue(all["includeSystem"]);
      assertTrue(all["requireFromPresent"]);
      assertTrue(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 22.12);
      assertEqual(all["initialSyncMaxWaitTime"], 12.21);
      assertEqual(all["idleMinWaitTime"], 1.4);
      assertEqual(all["idleMaxWaitTime"], 7.3);


      body = { "endpoint" : "ssl://127.0.0.1:12345", "database" : "bar", "ignoreErrors" : 2, "requestTimeout" : 12.5, "connectTimeout" : 26.3, "maxConnectRetries" : 12, "chunkSize" : 1234567, "autoStart" : false, "adaptivePolling" : true, "autoResync" : false, "includeSystem" : false, "requireFromPresent" : false, "verbose" : false, "connectionRetryWaitTime" : 2.5, "initialSyncMaxWaitTime" : 4.3, "idleMinWaitTime" : 0.22, "idleMaxWaitTime" : 3.5 };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      all = doc.parsedBody;
      assertEqual(all["endpoint"], "ssl://127.0.0.1:12345");
      assertEqual(all["database"], "bar");
      assertEqual(all["requestTimeout"], 12.5);
      assertEqual(all["connectTimeout"], 26.3);
      assertEqual(all["ignoreErrors"], 2);
      assertEqual(all["maxConnectRetries"], 12);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 1234567);
      assertFalse(all["autoStart"]);
      assertTrue(all["adaptivePolling"]);
      assertFalse(all["autoResync"]);
      assertFalse(all["includeSystem"]);
      assertFalse(all["requireFromPresent"]);
      assertFalse(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 2.5);
      assertEqual(all["initialSyncMaxWaitTime"], 4.3);
      assertEqual(all["idleMinWaitTime"], 0.22);
      assertEqual(all["idleMaxWaitTime"], 3.5);

      // refetch same data;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      all = doc.parsedBody;
      assertEqual(all["endpoint"], "ssl://127.0.0.1:12345");
      assertEqual(all["database"], "bar");
      assertEqual(all["requestTimeout"], 12.5);
      assertEqual(all["connectTimeout"], 26.3);
      assertEqual(all["ignoreErrors"], 2);
      assertEqual(all["maxConnectRetries"], 12);
      assertEqual(all["sslProtocol"], 0);
      assertEqual(all["chunkSize"], 1234567);
      assertFalse(all["autoStart"]);
      assertTrue(all["adaptivePolling"]);
      assertFalse(all["autoResync"]);
      assertFalse(all["includeSystem"]);
      assertFalse(all["requireFromPresent"]);
      assertFalse(all["verbose"]);
      assertEqual(all["connectionRetryWaitTime"], 2.5);
      assertEqual(all["initialSyncMaxWaitTime"], 4.3);
      assertEqual(all["idleMinWaitTime"], 0.22);
      assertEqual(all["idleMaxWaitTime"], 3.5);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // state;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_applier_state_again_2: function() {
      // fetch state;
      let cmd = api + "/applier-state";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('state'));
      assertTrue(all.hasOwnProperty('server'));

      let state = all['state'];
      assertFalse(state['running']);
      assertTrue(state.hasOwnProperty("lastAppliedContinuousTick"));
      assertTrue(state.hasOwnProperty("lastProcessedContinuousTick"));
      assertTrue(state.hasOwnProperty("lastAvailableContinuousTick"));
      assertTrue(state.hasOwnProperty("safeResumeTick"));

      assertTrue(state.hasOwnProperty("progress"));
      let progress = state['progress'];
      assertTrue(progress.hasOwnProperty("time"));
      assertMatch(/^([0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z)?$/, progress['time']);
      assertTrue(progress.hasOwnProperty("failedConnects"));

      assertTrue(state.hasOwnProperty("totalRequests"));
      assertTrue(state.hasOwnProperty("totalFailedConnects"));
      assertTrue(state.hasOwnProperty("totalEvents"));
      assertTrue(state.hasOwnProperty("totalOperationsExcluded"));

      assertTrue(state.hasOwnProperty("lastError"));
      let lastError = state["lastError"];
      assertTrue(lastError.hasOwnProperty("errorNum"));
      assertTrue(state.hasOwnProperty("time"));
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// logger;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_logger_Suite () {
  return {
    setUp: function() {
      db._drop("UnitTestsReplication");
      let res = db._createDatabase("UnitTestDB");;
      assertTrue(res);
      arango.PUT_RAW(api + "/applier-stop", "");
      arango.DELETE_RAW(api + "/applier-state", "");
    },

    tearDown: function() {
      let res = db._dropDatabase("UnitTestDB");;
      assertTrue(res);
      arango.PUT_RAW(api + "/applier-stop", "");
      arango.DELETE_RAW(api + "/applier-state", "");
      db._drop("UnitTestsReplication");
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // state;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_state: function() {
      // fetch state;
      let cmd = api + "/logger-state";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('state'));
      assertTrue(all.hasOwnProperty('server'));
      assertTrue(all.hasOwnProperty('clients'));

      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      let server = all['server'];
      assertMatch(/^[0-9]+$/, server['serverId']);
      assertTrue(server.hasOwnProperty('version'));
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // follow;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_empty_follow_log_2: function() {
      while (true) {
        let cmd = api + "/logger-state";
        let doc = arango.GET_RAW(cmd);
        assertEqual(doc.code, 200);
        assertTrue(doc.parsedBody["state"]["running"]);
        let fromTick = doc.parsedBody["state"]["lastLogTick"];

        cmd = api + "/logger-follow?from=" + fromTick;
        doc = arango.GET_RAW(cmd);

        if (doc.code !== 204) {
          // someone else did something else;
          assertEqual(doc.code, 200);
          // sleep(for a second && try again;
          sleep(1);
        } else {
          assertEqual(doc.code, 204);

          //assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
          assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
          assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
          assertEqual(doc.headers["content-type"], "application/x-arango-dump");

          let body = doc.body;
          assertEqual(body, undefined);
          break;
        }
      }
    },

    test_fetches_a_create_collection_action_from_the_follow_log_2: function() {
      db._useDatabase("UnitTestDB");
      db._drop("UnitTestsReplication");

      sleep(5);

      let cmd = api + "/logger-state";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["state"]["running"]);
      let fromTick = doc.parsedBody["state"]["lastLogTick"];

      
      let cid = db._create("UnitTestsReplication", { waitForSync: true });
      db._useDatabase("_system");

      sleep(5);

      cmd = api + "/logger-follow?from=" + fromTick;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();

      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);
        let document = JSON.parse(part);

        if (document["type"] === 2000 && document["cid"] === cid._id) {
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));
          assertTrue(document.hasOwnProperty("cname"));
          assertTrue(document.hasOwnProperty("data"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2000);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');
          assertEqual(document["cname"], "UnitTestsReplication");

          let c = document["data"];
          assertTrue(c.hasOwnProperty("version"));
          assertEqual(c["type"], 2);
          assertEqual(c["cid"], cid._id);
          assertEqual(typeof c["cid"], 'string');
          assertFalse(c["deleted"]);
          assertEqual(c["name"], "UnitTestsReplication");
          assertTrue(c["waitForSync"]);
        }

        body = body.slice(position + 1, body.length);
      }

    },

    test_fetches_some_collection_operations_from_the_follow_log_2: function() {
      db._drop("UnitTestsReplication", "UnitTestDB");

      sleep(5);

      let cmd = api + "/logger-state";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["state"]["running"]);
      let fromTick = doc.parsedBody["state"]["lastLogTick"];

      // create collection;
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication", { waitForSync: true});
      db._useDatabase("_system");

      // create document;
      cmd = "/_db/UnitTestDB/_api/document?collection=UnitTestsReplication";
      let body = { "_key" : "test", "test" : false };
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      let rev = doc.parsedBody["_rev"];

      // delete document;
      cmd = "/_db/UnitTestDB/_api/document/UnitTestsReplication/test";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);

      // drop collection;
      cmd = "/_db/UnitTestDB/_api/collection/UnitTestsReplication";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);

      sleep(5);

      cmd = api + "/logger-follow?from=" + fromTick;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);
        let document = JSON.parse(part);

        if (i === 0) {
          if (document["type"] === 2000 && document["cid"] === cid._id) {
            // create collection;
            assertTrue(document.hasOwnProperty("tick"));
            assertTrue(document.hasOwnProperty("type"));
            assertTrue(document.hasOwnProperty("cid"));
            assertTrue(document.hasOwnProperty("cname"));
            assertTrue(document.hasOwnProperty("data"));

            assertMatch(/^[0-9]+$/, document["tick"]);
            assertTrue(Number(document["tick"]) >= Number(fromTick));
            assertEqual(document["type"], 2000);
            assertEqual(document["cid"], cid._id);
            assertEqual(typeof document["cid"], 'string');
            assertEqual(document["cname"], "UnitTestsReplication");

            let c = document["data"];
            assertTrue(c.hasOwnProperty("version"));
            assertEqual(c["type"], 2);
            assertEqual(c["cid"], cid._id);
            assertEqual(typeof c["cid"], 'string');
            assertFalse(c["deleted"]);
            assertEqual(c["name"], "UnitTestsReplication");
            assertTrue(c["waitForSync"]);

            i = i + 1;
          }
        } else if ( i === 1 && document["type"] === 2300 && document["cid"] === cid._id) {
          // create document;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2300);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');
          assertEqual(document["data"]["_key"], "test");
          assertMatch(/^[a-zA-Z0-9_\-]+$/, document["data"]["_rev"]);
          assertNotEqual(document["data"]["_rev"], "0");
          assertFalse(document["data"]["test"]);

          i = i + 1;
        } else if ( i === 2 && document["type"] === 2302 && document["cid"] === cid._id) {
          // delete document;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2302);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');
          assertEqual(document["data"]["_key"], "test");

          i = i + 1;
        } else if ( i === 3 && document["type"] === 2001 && document["cid"] === cid._id) {
          // drop collection;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2001);
          assertEqual(document["cid"], cid._id);
          assertEqual(typeof document["cid"], 'string');

          i = i + 1;
        }

        body = body.slice(position + 1, body.length);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// inventory / dump;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_initial_dumSuite () {
  let batchId;
  return {
    setUp: function() {
      db._createDatabase("UnitTestDB");
      let doc = arango.POST_RAW(api + "/batch", "{}");
      assertEqual(doc.code, 200);
      batchId = doc.parsedBody['id'];
      assertMatch(/^[0-9]+$/, batchId);
    },

    tearDown: function() {
      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      db._drop("UnitTestsReplication", "UnitTestDB");
      db._drop("UnitTestsReplication2", "UnitTestDB");
      db._useDatabase("_system");
      db._dropDatabase("UnitTestDB");
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // inventory;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_initial_inventory_2: function() {
      let cmd = api + `/inventory?includeSystem=false&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let collections = all["collections"];
      let filtered = [ ];
      collections.forEach(collection => {
        if ([ "UnitTestsReplication", "UnitTestsReplication2" ].find( name => {
          return name === collection["parameters"]["name"];
        }) !== undefined) {
          filtered.push(collection);
        }
      });
      assertEqual(filtered, [ ]);
      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);
    },

    test_checks_the_initial_inventory_for_non_system_collections_2: function() {
      let cmd = api + `/inventory?includeSystem=false&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let collections = all["collections"];
      collections.forEach(collection => {
        assertNotMatch(/^_/, collection["parameters"]["name"]);
      });
    },

    test_checks_the_initial_inventory_for_system_collections_2: function() {
      let cmd = api + `/inventory?includeSystem=true&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let collections = all["collections"];
      let systemCollections = 0;
      collections.forEach(collection => {
        if (collection["parameters"]["name"].match(/^_/))
          systemCollections = systemCollections + 1;
      });

      assertNotEqual(systemCollections, 0);
    },

    test_checks_the_inventory_after_creating_collections_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._createEdgeCollection("UnitTestsReplication2", {waitForSync: true });

      let cmd = api + `/inventory?includeSystem=false&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      let collections = all['collections'];
      let filtered = [ ];
      collections.forEach(collection => {
        if ([ "UnitTestsReplication", "UnitTestsReplication2" ].find( name => {
          return name === collection["parameters"]["name"];
        }) !== undefined) {
          filtered.push(collection);
        }
      });
      assertEqual(filtered.length, 2, collections);

      // first collection;
      let c = filtered[0];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      let parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 2);
      assertEqual(parameters["cid"], cid._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication");
      assertFalse(parameters["waitForSync"]);

      assertEqual(c['indexes'], [ ]);

      // second collection;
      c = filtered[1];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 3);
      assertEqual(parameters["cid"], cid2._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication2");
      assertTrue(parameters["waitForSync"]);

      assertEqual(c['indexes'], [ ]);
    },

    test_checks_the_inventory_with_indexes_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._create("UnitTestsReplication2");

      let idxUrl = "/_db/UnitTestDB/_api/index";

      let body = { "type" : "hash", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication`, body);
      assertEqual(doc.code, 201);

      body = { "type" : "skiplist", "unique" : false, "fields" : [ "c" ] };
      doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication`, body);
      assertEqual(doc.code, 201);

      // create indexes for second collection;
      body = { "type" : "geo", "unique" : false, "fields" : [ "a", "b" ] };
      doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication2`, body);
      assertEqual(doc.code, 201);

      body = { "type" : "skiplist", "unique" : true, "fields" : [ "d" ] };
      doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication2`, body);
      assertEqual(doc.code, 201);

      body = { "type" : "fulltext", "minLength" : 8, "fields" : [ "ff" ] };
      doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication2`, body);
      assertEqual(doc.code, 201);

      let cmd = api + `/inventory?batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      let all = doc.parsedBody;
      assertTrue(all.hasOwnProperty('collections'));
      assertTrue(all.hasOwnProperty('state'));

      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      let collections = all['collections'];
      let filtered = [ ];
      collections.forEach(collection => {
        if ([ "UnitTestsReplication", "UnitTestsReplication2" ].find( name => {
          return name === collection["parameters"]["name"];
        }) !== undefined) {
          filtered.push(collection);
        }
      });
      assertEqual(filtered.length, 2);

      // first collection;
      let c = filtered[0];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      let parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 2);
      assertEqual(parameters["cid"], cid._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication");
      assertFalse(parameters["waitForSync"]);

      let indexes = c['indexes'];
      assertEqual(indexes.length, 2);

      let idx = indexes[0];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "hash");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "a", "b" ]);

      idx = indexes[1];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "skiplist");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "c" ]);

      // second collection;
      c = filtered[1];
      assertTrue(c.hasOwnProperty("parameters"));
      assertTrue(c.hasOwnProperty("indexes"));

      parameters = c['parameters'];
      assertTrue(parameters.hasOwnProperty("version"));
      assertEqual(typeof parameters["version"], 'number');
      assertEqual(typeof parameters["type"], 'number');
      assertEqual(parameters["type"], 2);
      assertEqual(parameters["cid"], cid2._id);
      assertEqual(typeof parameters["cid"], 'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication2");
      assertFalse(parameters["waitForSync"]);

      indexes = c['indexes'];
      assertEqual(indexes.length, 3);

      idx = indexes[0];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "geo");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "a", "b" ]);

      idx = indexes[1];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "skiplist");
      assertTrue(idx["unique"]);
      assertEqual(idx["fields"], [ "d" ]);

      idx = indexes[2];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "fulltext");
      assertFalse(idx["unique"]);
      assertEqual(idx["minLength"], 8);
      assertEqual(idx["fields"], [ "ff" ]);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // dump;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_the_dump_for_an_empty_collection_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 204);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");
      assertEqual(doc.body, undefined);
    },

    test_checks_the_dump_for_a_non_empty_collection_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 100; i++ ) {
        let body = { "_key" : `test${i}`, "test" : `${i}`};
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202, doc);
      }

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200, doc);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let doc = JSON.parse(part);
        assertEqual(doc['type'], 2300);
        assertMatch(/^test[0-9]+$/, doc['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, doc["data"]["_rev"]);
        assertTrue(doc['data'].hasOwnProperty("test"));

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertEqual(i, 100);
    },

    test_checks_the_dump_for_a_non_empty_collection__small_chunkSize_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 100; i++ ) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      }

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&chunkSize=1024&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "true");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let doc = JSON.parse(part);
        assertEqual(doc['type'], 2300);
        assertMatch(/^test[0-9]+$/, doc['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, doc["data"]["_rev"]);
        assertTrue(doc['data'].hasOwnProperty('test'));

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertTrue(i < 100);
    },


    test_checks_the_dump_for_an_edge_collection_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._createEdgeCollection("UnitTestsReplication2");

      for (let i = 0; i < 100; i++ ) {
        let body = { "_key" : `test${i}`, "_from" : "UnitTestsReplication/foo", "_to" : "UnitTestsReplication/bar", "test1" : `${i}`, "test2" : false, "test3" : [ ], "test4" : { } };
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication2", body);
        assertEqual(doc.code, 202, doc);
      }

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication2&chunkSize=65536&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let document = JSON.parse(part);
        assertEqual(document['type'], 2300);
        assertMatch(/^test[0-9]+$/, document['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, document["data"]["_rev"]);
        assertEqual(document['data']['_from'], "UnitTestsReplication/foo");
        assertEqual(document['data']['_to'], "UnitTestsReplication/bar");
        assertTrue(document['data'].hasOwnProperty('test1'));
        assertFalse(document['data']['test2']);
        assertEqual(document['data']['test3'], [ ]);
        assertEqual(document['data']['test4'], { });

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertEqual(i, 100);
    },

    test_checks_the_dump_for_an_edge_collection__small_chunkSize_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");
      let cid2 = db._createEdgeCollection("UnitTestsReplication2");

      for (let i = 0; i < 100; i++ ) {
        let body = { "_key" : `test${i}`, "_from" : "UnitTestsReplication/foo", "_to" : "UnitTestsReplication/bar", "test1" : `${i}`, "test2" : false, "test3" : [ ], "test4" : { } };
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication2", body);
        assertEqual(doc.code, 202);
      }

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication2&chunkSize=1024&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200, doc);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "true");
      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let document = JSON.parse(part);
        assertEqual(document['type'], 2300);
        assertMatch(/^test[0-9]+$/, document['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, document['data']['_rev']);
        assertEqual(document['data']['_from'], "UnitTestsReplication/foo");
        assertEqual(document['data']['_to'], "UnitTestsReplication/bar");
        assertTrue(document['data'].hasOwnProperty('test1'));
        assertFalse(document['data']['test2']);
        assertEqual(document['data']['test3'], [ ]);
        assertEqual(document['data']['test4'], { });

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertTrue(i< 100);
    },

    test_checks_the_dump_for_a_collection_with_deleted_documents_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      for (let i = 0; i < 10; i++ ) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
        
        doc = arango.DELETE_RAW(`/_db/UnitTestDB/_api/document/UnitTestsReplication/test${i}`, body);
        assertEqual(doc.code, 202);
      }

      doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 204, doc);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");
    },

    test_checks_the_dump_for_a_truncated_collection_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 10; i++ ) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      }

      // truncate;
      let cmd = "/_db/UnitTestDB/_api/collection/UnitTestsReplication/truncate";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);

      doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 204);

      assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
      assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");
    },

    test_checks_the_dump_for_a_non_empty_collection__3_0_mode_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 100; i++ ) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      }

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      let cmd = api + `/dump?collection=UnitTestsReplication&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200, doc);

      let body = doc.body.toString();
      let i = 0;
      while (body.length > 1) {
        let position = body.search("\n");

        if (position === undefined) {
          break;
        }

        let part = body.slice(0, position);

        let doc = JSON.parse(part);
        assertEqual(doc['type'], 2300);
        assertFalse(doc.hasOwnProperty("key"));
        assertFalse(doc.hasOwnProperty("rev"));
        assertMatch(/^test[0-9]+$/, doc['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, doc['data']['_rev']);
        assertTrue(doc['data'].hasOwnProperty('test'));

        body = body.slice(position + 1, body.length);
        i = i + 1;
      }
      assertEqual(i, 100);
    },

    test_fetches_incremental_parts_of_a_collection_dump_2: function() {
      db._useDatabase("UnitTestDB");
      let cid = db._create("UnitTestsReplication");

      for (let i = 0; i < 10; i++ ) {
        let body = { "_key" : `test${i}`, "test" : `${i}` };
        let doc = arango.POST_RAW("/_db/UnitTestDB/_api/document?collection=UnitTestsReplication", body);
        assertEqual(doc.code, 202);
      };

      let doc = arango.PUT_RAW("/_admin/wal/flush?waitForSync=true&waitForCollector=true", "");
      assertEqual(doc.code, 200);

      let fromTick = 0;

      arango.DELETE_RAW(api + `/batch/${batchId}`, "");
      let doc0 = arango.POST_RAW(api + "/batch", "{}");
      batchId = doc0.parsedBody["id"];
      assertMatch(/^[0-9]+$/, batchId);

      for (let i = 0; i < 10; i++ ) {
        let cmd = api + `/dump?collection=UnitTestsReplication&from=${fromTick}&chunkSize=1&batchId=${batchId}`;
        let doc = arango.GET_RAW(cmd);
        assertEqual(doc.code, 200, doc);

        if (i === 9) {
          assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
        } else {
          assertEqual(doc.headers["x-arango-replication-checkmore"], "true");
        }

        assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
        assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
        assertTrue(Number(doc.headers["x-arango-replication-lastincluded"]) >= Number(fromTick));
        assertEqual(doc.headers["content-type"], "application/x-arango-dump");

        fromTick = doc.headers["x-arango-replication-lastincluded"];

        let body = doc.body.toString();
        let document = JSON.parse(body);
        assertEqual(document['type'], 2300);
        assertMatch(/^test[0-9]+$/, document['data']['_key']);
        assertMatch(/^[a-zA-Z0-9_\-]+$/, document['data']['_rev']);
        assertTrue(document['data'].hasOwnProperty('test'));
      };
    }
  };
}

jsunity.run(dealing_with_general_function_interfaceSuite);
jsunity.run(dealing_with_the_applier_interfaceSuite);
jsunity.run(dealing_with_the_loggerSuite);
jsunity.run(dealing_with_batches_and_logger_stateSuite);
jsunity.run(dealing_with_the_initial_dump_interfaceSuite);

jsunity.run(dealing_with_general_functionSuite);
jsunity.run(dealing_with_the_applierSuite);
jsunity.run(dealing_with_the_logger_Suite);
jsunity.run(dealing_with_the_initial_dumSuite);
return jsunity.done();
