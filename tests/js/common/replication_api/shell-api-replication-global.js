/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertMatch, assertNotEqual */

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json; charset=utf-8" :  "application/x-velocypack";
const jsunity = require("jsunity");


////////////////////////////////////////////////////////////////////////////////;
// applier;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_applierSuite () {
  let api = "/_api/replication";

  return {
    setUp: function() {
      arango.PUT_RAW(api + "/applier-stop?global=true", "");
      arango.DELETE_RAW(api + "/applier-state?global=true", "");
    },

    tearDown: function() {
      arango.PUT_RAW(api + "/applier-stop?global=true", "");
      arango.DELETE_RAW(api + "/applier-state?global=true", "");
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // start;
    ////////////////////////////////////////////////////////////////////////////////;

    test_starts_the_applier: function() {
      let cmd = api + "/applier-start?global=true";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 400); //  because configuration is invalid
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // stop;
    ////////////////////////////////////////////////////////////////////////////////;

    test_stops_the_applier: function() {
      let cmd = api + "/applier-stop?global=true";
      let doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // properties;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_applier_config: function() {
      let cmd = api + "/applier-config?global=true";
      let doc = arango.GET_RAW(cmd, "");

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
      assertEqual(typeof all["restrictType"],'string');
      assertEqual(typeof all["connectionRetryWaitTime"], 'number');
      assertEqual(typeof all["initialSyncMaxWaitTime"], 'number');
      assertEqual(typeof all["idleMinWaitTime"], 'number');
      assertEqual(typeof all["idleMaxWaitTime"], 'number');
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // set and fetch properties;
    ////////////////////////////////////////////////////////////////////////////////;

    test_sets_and_re_fetches_the_applier_config: function() {
      let cmd = api + "/applier-config?global=true";
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
      doc = arango.GET_RAW(cmd, "");

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
      doc = arango.GET_RAW(cmd, "");

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

    test_checks_the_state: function() {
      // fetch state;
      let cmd = api + "/applier-state?global=true";
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
// wal access;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_wal_access_apiSuite () {
  let api = "/_api/wal";
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

    test_check_the_state: function() {
      // fetch state;
      let cmd = "/_api/replication/logger-state";
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
    // tickRanges;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_available_tick_range_1: function() {
      // fetch state;
      let cmd = api + "/range";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      let result = doc.parsedBody;
      assertTrue(doc.body.length > 0, doc);

      assertTrue(result.hasOwnProperty('tickMin'));
      assertTrue(result.hasOwnProperty('tickMax'));
      assertMatch(/^[0-9]+$/, result['tickMin']);
      assertMatch(/^[0-9]+$/, result['tickMax']);
      assertTrue(result.hasOwnProperty('server'));
      assertTrue(result['server'].hasOwnProperty('serverId'));
      assertMatch(/^[0-9]+$/, result['server']['serverId']);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // lastTick;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_available_tick_range_2: function() {
      // fetch state;
      let cmd = api + "/lastTick";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);

      let result = doc.parsedBody;
      assertTrue(doc.body.length > 0, doc);

      assertTrue(result.hasOwnProperty('tick'));
      assertMatch(/^[0-9]+$/, result['tick']);
      assertTrue(result.hasOwnProperty('server'));
      assertTrue(result['server'].hasOwnProperty('serverId'));
      assertMatch(/^[0-9]+$/, result['server']['serverId']);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // follow;
    ////////////////////////////////////////////////////////////////////////////////;

    test_fetches_the_empty_follow_log: function() {
      while (true) {
        let cmd = api + "/lastTick";
        let doc = arango.GET_RAW(cmd);
        assertEqual(doc.code, 200);
        let fromTick = doc.parsedBody["tick"];

        cmd = api + "/tail?global=true&from=" + fromTick;
        doc = arango.GET_RAW(cmd);

        if (doc.code !== 204) {
          // someone else did something else;
          assertEqual(doc.code, 200);
          // sleep for a second and try again;
          sleep(1);
        } else {
          assertEqual(doc.code, 204);

          // TODO assertEqual(doc.headers["x-arango-replication-checkmore"], "false");
          assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
          assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
          assertEqual(doc.headers["content-type"], "application/x-arango-dump");

          let body = doc.parsedBody;
          assertEqual(body, undefined, doc);
          break;
        }
      }
    },

    test_tails_the_WAL_with_a_tick_far_in_the_future: function() {
      let cmd = api + "/lastTick";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      let fromTick = Number(doc.parsedBody["tick"]) * 10000000;

      cmd = api + "/tail?global=true&from=" + fromTick;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 204);
      assertEqual(doc.headers["x-arango-replication-lastincluded"], "0");
    },

    test_fetches_a_create_collection_action_from_the_follow_log: function() {
      let cid = 0;
      let cuid = 0;
      let doc = {};
      let fromTick;

      while (true) {
        db._drop("UnitTestsReplication");

        sleep(5);

        let cmd = api + "/lastTick";
        doc = arango.GET_RAW(cmd);
        assertEqual(doc.code, 200);
        fromTick = doc.parsedBody["tick"];

        cid = db._create("UnitTestsReplication", { waitForSync: true } );
        cuid = cid.properties()["globallyUniqueId"];

        sleep(1);

        cmd = api + "/tail?global=true&from=" + fromTick;
        doc = arango.GET_RAW(cmd);
        assertTrue(doc.code === 200 || doc.code === 204);

        if (doc.headers["x-arango-replication-frompresent"] === "true" &&
            doc.headers["x-arango-replication-lastincluded"] !== "0") {
          break;
        }
      }

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let body = doc.body.toString();
      while (true) {
        let position = body.search("\n");
        if (position === -1) {
          break;
        }

        let part = JSON.parse(body.slice(0, position));

        if (part["type"] === 2000 && part["cuid"] === cuid) {
          assertTrue(part.hasOwnProperty("tick"));
          assertTrue(part.hasOwnProperty("type"));
          assertTrue(part.hasOwnProperty("cuid"));
          assertTrue(part.hasOwnProperty("data"));

          assertMatch(/^[0-9]+$/, part["tick"]);
          assertTrue(Number(part["tick"]) >= Number(fromTick));
          assertEqual(part["type"], 2000);
          assertEqual(part["cuid"], cuid);

          let c = part["data"];
          assertTrue(c.hasOwnProperty("version"));
          assertEqual(c["type"], 2);
          assertEqual(c["cid"], cid._id);
          assertEqual(typeof c["cid"],'string');
          assertEqual(c["globallyUniqueId"], cuid);
          assertFalse(c["deleted"]);
          assertEqual(c["name"], "UnitTestsReplication");
          assertTrue(c["waitForSync"]);
        }

        body = body.slice(position + 1, body.length);
      }
    },

    test_fetches_some_collection_operations_from_the_follow_log: function() {
      let doc;
      let body;
      let cid;
      let cuid;
      let rev;
      let rev2;
      let fromTick;
      while (true) {
        db._drop("UnitTestsReplication");

        sleep(5);

        let cmd = api + "/lastTick";
        doc = arango.GET_RAW(cmd);
        assertEqual(doc.code, 200);
        fromTick = doc.parsedBody["tick"];

        // create collection;
        cid = db._create("UnitTestsReplication", { waitForSync: true, globallyUniqueId: true });
        cuid = cid.properties()["globallyUniqueId"];

        // create document;
        cmd = "/_api/document?collection=UnitTestsReplication";
        body = { "_key" : "test", "test" : false };
        doc = arango.POST_RAW(cmd, body);
        assertEqual(doc.code, 201, doc);
        rev = doc.parsedBody["_rev"];

        // update document;
        cmd = "/_api/document/UnitTestsReplication/test";
        body = { "updated" : true };
        doc = arango.PATCH_RAW(cmd, body);
        assertEqual(doc.code, 201);
        rev2 = doc.parsedBody["_rev"];

        // delete document;
        cmd = "/_api/document/UnitTestsReplication/test";
        doc = arango.DELETE_RAW(cmd);
        assertEqual(doc.code, 200);

        sleep(1);

        cmd = api + "/tail?global=true&from=" + fromTick;
        doc = arango.GET_RAW(cmd);
        assertTrue(doc.code === 200 || doc.code === 204);
        body = doc.body.toString();

        if (doc.headers["x-arango-replication-frompresent"] === "true" &&
            doc.headers["x-arango-replication-lastincluded"] !== "0") {
          break;
        }
      }

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let i = 0;
      while (true) {
        let position = body.search("\n");

        if (position === -1) {
          break;
        }

        let part = body.slice(0, position);
        let document = JSON.parse(part);

        if (i === 0) {
          if (document["type"] === 2000 && document["cuid"] === cuid) {
            // create collection;
            assertTrue(document.hasOwnProperty("tick"));
            assertTrue(document.hasOwnProperty("type"));
            assertTrue(document.hasOwnProperty("cuid"));
            assertTrue(document.hasOwnProperty("data"));

            assertMatch(/^[0-9]+$/, document["tick"]);
            assertTrue(Number(document["tick"]) >= Number(fromTick), document);
            assertEqual(document["type"], 2000);
            assertEqual(document["cuid"], cuid);

            let c = document["data"];
            assertTrue(c.hasOwnProperty("version"));
            assertEqual(c["type"], 2);
            assertEqual(c["cid"], cid._id);
            assertEqual(typeof c["cid"],'string');
            assertEqual(c["globallyUniqueId"], cuid);
            assertFalse(c["deleted"]);
            assertEqual(c["name"], "UnitTestsReplication");
            assertTrue(c["waitForSync"]);

            i = i + 1;
          }
        } else if (i === 1 && document["type"] === 2300 && document["cuid"] === cuid) {
          // create document;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cuid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2300);
          assertEqual(document["cuid"], cuid);
          assertEqual(document["data"]["_key"], "test");
          assertMatch(/^[a-zA-Z0-9_\-]+$/, document["data"]["_rev"]);
          assertEqual(document["data"]["_rev"], rev);
          assertFalse(document["data"]["test"]);

          i = i + 1;
        } else if (i === 2 && document["cuid"] === cuid) {
          // update document, there must only be 2300 no 2302;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cuid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2300);
          assertEqual(document["cuid"], cuid);
          assertEqual(document["data"]["_key"], "test");
          assertMatch(/^[a-zA-Z0-9_\-]+$/, document["data"]["_rev"]);
          assertEqual(document["data"]["_rev"], rev2);
          assertFalse(document["data"]["test"]);

          i = i + 1;
        } else if (i === 3 && document["type"] === 2302 && document["cuid"] === cuid) {
          // delete document;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cuid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2302);
          assertEqual(document["cuid"], cuid);
          assertEqual(document["data"]["_key"], "test");
          assertMatch(/^[a-zA-Z0-9_\-]+$/, document["data"]["_rev"]);
          assertNotEqual(document["data"]["_rev"], rev);

          i = i + 1;
        }

        body = body.slice(position + 1, body.length);
      }

      assertEqual(i, 4);

      // tail you later;
      let cmd = api + "/lastTick";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      fromTick = doc.parsedBody["tick"];

      // drop collection;
      cmd = "/_api/collection/UnitTestsReplication";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);

      while (true) {
        sleep(1);
        cmd = api + "/tail?global=true&from=" + fromTick;
        doc = arango.GET_RAW(cmd);
        assertTrue(doc.code === 200 || doc.code === 204);
        body = doc.body.toString();

        if (doc.headers["x-arango-replication-frompresent"] === "true" &&
            doc.headers["x-arango-replication-lastincluded"] !== "0") {
          break;
        }
      }

      while (true) {
        let position = body.search("\n");

        if (position === -1) {
          break;
        }

        let part = body.slice(0, position);
        let document = JSON.parse(part);

        if (document["type"] === 2001 && document["cuid"] === cuid) {
          // drop collection;
          assertTrue(document.hasOwnProperty("tick"));
          assertTrue(document.hasOwnProperty("type"));
          assertTrue(document.hasOwnProperty("cuid"));

          assertMatch(/^[0-9]+$/, document["tick"]);
          assertTrue(Number(document["tick"]) >= Number(fromTick));
          assertEqual(document["type"], 2001);
          assertEqual(document["cuid"], cuid);

          i = i + 1;
        }

        body = body.slice(position + 1, body.length);
      }

      assertEqual(i, 5);
    },

    test_fetches_some_more_single_document_operations_from_the_log: function() {
      db._drop("UnitTestsReplication");

      sleep(5);

      let cmd = api + "/lastTick";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      let fromTick = doc.parsedBody["tick"];

      // create collection;
      let cid = db._create("UnitTestsReplication", {waitForSync: true});
      let cuid = cid.properties()["globallyUniqueId"];

      // create document;
      cmd = "/_api/document?collection=UnitTestsReplication";
      let body = { "_key" : "test", "test" : false };
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201, doc);
      let rev = doc.parsedBody["_rev"];

      // update document;
      cmd = "/_api/document/UnitTestsReplication/test";
      body = { "updated" : true };
      doc = arango.PATCH_RAW(cmd, body);
      assertEqual(doc.code, 201);
      let rev2 = doc.parsedBody["_rev"];

      // delete document;
      cmd = "/_api/document/UnitTestsReplication/test";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);

      sleep(5);

      cmd = api + "/tail?global=true&from=" + fromTick;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      let tickTypes = { 2000: 0, 2001: 0, 2300: 0, 2302: 0 };

      body = doc.body.toString();

      cmd = api + "/lastTick";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      let i = 0;
      while (true) {
        let position = body.search("\n");

        if (position === -1) {
          break;
        }

        let part = body.slice(0, position);
        let marker = JSON.parse(part);

        if (marker["type"] >= 2000 && marker["cuid"] === cuid) {
          // create collection;
          assertTrue(marker.hasOwnProperty("tick"));
          assertTrue(marker.hasOwnProperty("type"));
          assertTrue(marker.hasOwnProperty("cuid"));

          if (marker["type"] === 2300) {
            assertTrue(marker.hasOwnProperty("data"));
          }
          let cc = tickTypes[marker["type"]];
          tickTypes[marker["type"]] = cc + 1;
        }
        body = body.slice(position + 1, body.length);
      }

      assertEqual(tickTypes[2000], 1); // collection create
      assertEqual(tickTypes[2001], 0); // collection drop
      assertEqual(tickTypes[2300], 2); // document insert / update
      assertEqual(tickTypes[2302], 1); // document drop

      // drop collection;
      cmd = "/_api/collection/UnitTestsReplication";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);
    },

    test_validates_chunkSize_restrictions: function() {
      db._drop("UnitTestsReplication");

      sleep(1);

      let cmd = api + "/lastTick";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      let fromTick = doc.parsedBody["tick"];
      let originalTick = fromTick;
      let lastScanned = fromTick;

      // create collection;
      let cid = db._create("UnitTestsReplication", { waitForSync: true });
      let cuid = cid.properties()["globallyUniqueId"];

      // create documents;
      for (let value = 0; value < 250; value ++) {
        cmd = "/_api/document?collection=UnitTestsReplication";
        let body = { "value" : "thisIsALongerStringBecauseWeWantToTestTheChunkSizeLimitsLaterOnAndItGetsEvenLongerWithTimeForRealNow" };
        doc = arango.POST_RAW(cmd, body);
        assertEqual(doc.code, 201);
      }

      // create one big transaction;
      let docsBody = [];
      for (let value=0; value < 749; value++) {
        docsBody.push({ "value" : value });
      }
      docsBody.push({ "value" : "500" });
      cmd = "/_api/document?collection=UnitTestsReplication";
      doc = arango.POST_RAW(cmd, docsBody);
      assertEqual(doc.code, 201);

      // create more documents;
      for(let value = 0; value < 500; value ++) {
        cmd = "/_api/document?collection=UnitTestsReplication";
        let body = { "value" : "thisIsALongerStringBecauseWeWantToTestTheChunkSizeLimitsLaterOnAndItGetsEvenLongerWithTimeForRealNow" };
        doc = arango.POST_RAW(cmd, body);
        assertEqual(doc.code, 201);
      }

      sleep(1);

      let tickTypes = { 2000: 0, 2001: 0, 2200: 0, 2201: 0, 2300: 0 };

      while (true) {
        cmd = api + "/tail?global=true&from=" + fromTick + "&lastScanned=" + lastScanned + "&chunkSize=16384";
        doc = arango.GET_RAW(cmd);
        assertTrue(doc.code === 200 || doc.code === 204);
        //print(doc)
        if (doc.code === 204) {
          break;
        }
        assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
        assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
        assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastscanned"]);
        assertNotEqual(doc.headers["x-arango-replication-lastscanned"], "0");
        if (fromTick === originalTick) {
          // first batch;
          assertEqual(doc.headers["x-arango-replication-checkmore"], "true");
        }
        assertEqual(doc.headers["content-type"], "application/x-arango-dump");
        // we need to allow for some overhead here, as the chunkSize restriction is not honored precisely;
        assertTrue(Number(doc.headers["content-length"]) <  (16 + 9) * 1024);

        // update lastScanned for next request;
        lastScanned = doc.headers["x-arango-replication-lastscanned"];
        let body = doc.body.toString();

        let i = 0;
        while (true) {
          let position = body.search("\n");

          if (position === -1) {
            break;
          }

          let part = body.slice(0, position);
          let marker = JSON.parse(part);

          // update last tick value;
          assertTrue(marker.hasOwnProperty("tick"));
          fromTick = marker["tick"];

          if (marker["type"] === 2200) {
            assertTrue(marker.hasOwnProperty("tid"));
            assertTrue(marker.hasOwnProperty("db"));
            tickTypes[2200] = tickTypes[2200] + 1;
          } else if (marker["type"] === 2201) {
            assertTrue(marker.hasOwnProperty("tid"));
            tickTypes[2201] = tickTypes[2201] + 1;
          } else if (marker["type"] >= 2000 && marker["cuid"] === cuid) {
            // collection markings;
            assertTrue(marker.hasOwnProperty("type"));
            assertTrue(marker.hasOwnProperty("cuid"));

            if (marker["type"] === 2300) {
              assertTrue(marker.hasOwnProperty("data"));
              assertTrue(marker.hasOwnProperty("tid"));
            }

            let cc = tickTypes[marker["type"]];
            tickTypes[marker["type"]] = cc + 1;

            if (tickTypes[2300] === 1500) {
              break;
            }
          }
          body = body.slice(position + 1, body.length);
        }
      }

      assertEqual(tickTypes[2000], 1, tickTypes); //  collection create
      assertEqual(tickTypes[2001], 0); //  collection drop
      assertTrue(tickTypes[2200] >= 1); // begin transaction
      assertTrue(tickTypes[2201] >= 1); // commit transaction
      assertEqual(tickTypes[2300], 1500); //  document inserts


      // now try again with a single chunk  ;
      tickTypes = { 2000: 0, 2001: 0, 2200: 0, 2201: 0, 2300: 0 };
      // big chunk: 1024x1024
      cmd = api + "/tail?global=true&from=" + originalTick + "&lastScanned=" + originalTick + "&chunkSize=1048576";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      assertMatch(/^[0-9]+$/, doc.headers["x-arango-replication-lastincluded"]);
      assertNotEqual(doc.headers["x-arango-replication-lastincluded"], "0");
      assertEqual(doc.headers["content-type"], "application/x-arango-dump");

      // we need to allow for some overhead here, as the chunkSize restriction is not honored precisely;
      assertTrue(Number(doc.headers["content-length"]) <  (1024 + 8) * 1024);

      let body = doc.body.toString();

      let i = 0;
      while (true) {
        let position = body.search("\n");

        if (position === -1) {
          break;
        }

        let part = body.slice(0, position);
        let marker = JSON.parse(part);

        assertTrue(marker.hasOwnProperty("tick"));

        if (marker["type"] === 2200) {
          assertTrue(marker.hasOwnProperty("tid"));
          assertTrue(marker.hasOwnProperty("db"));
          tickTypes[2200] = tickTypes[2200] + 1;
        } else if (marker["type"] === 2201) {
          assertTrue(marker.hasOwnProperty("tid"));
          tickTypes[2201] = tickTypes[2201] + 1;
        } else if (marker["type"] >= 2000 && marker["cuid"] === cuid) {
          // create collection;
          assertTrue(marker.hasOwnProperty("type"));
          assertTrue(marker.hasOwnProperty("cuid"));

          if (marker["type"] === 2300) {
            assertTrue(marker.hasOwnProperty("data"));
            assertTrue(marker.hasOwnProperty("tid"));
          }

          let cc = tickTypes[marker["type"]];
          tickTypes[marker["type"]] = cc + 1;

          if (tickTypes[2300] === 1500) {
            break;
          }
        }
        body = body.slice(position + 1, body.length);
      }

      assertEqual(tickTypes[2000], 1); //collection create
      assertEqual(tickTypes[2001], 0); //collection drop
      assertTrue(tickTypes[2200] >= 1); // begin transaction
      assertTrue(tickTypes[2201] >= 1); // commit transaction
      assertEqual(tickTypes[2300], 1500); //document inserts

      // drop collection;
      cmd = "/_api/collection/UnitTestsReplication";
      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// inventory / dump;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_initial_dumpSuite () {
  let api = "/_api/replication";
  let batchId;
  return {
    setUp: function() {
      db._drop("UnitTestsReplication");
      db._drop("UnitTestsReplication2");
      let doc = arango.POST_RAW(api + "/batch", "{}");
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

  test_checks_the_initial_inventory: function() {
    let cmd = api + `/inventory?includeSystem=true&global=true&batchId=${batchId}`;
    let doc = arango.GET_RAW(cmd);

    assertEqual(doc.code, 200);
    let all = doc.parsedBody;
    assertTrue(all.hasOwnProperty('databases'));
    assertTrue(all.hasOwnProperty('state'));
    let state = all['state'];
    assertTrue(state['running']);
    assertMatch(/^[0-9]+$/, state['lastLogTick']);
    assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

    let databases = all['databases'];
    for (const [name, database] of Object.entries(databases)) {
      assertTrue(database.hasOwnProperty('collections'));

      let collections = database["collections"];
      let filtered = [ ];
      
      collections.forEach(collection => {
        if ([ "UnitTestsReplication", "UnitTestsReplication2" ].includes(collection["parameters"]["name"])) {
          filtered.push(collection);
        }
        assertTrue(collection["parameters"].hasOwnProperty('globallyUniqueId'));
      });
      assertEqual(filtered, [ ]);
    }
  },

    test_checks_the_inventory_after_creating_collections: function() {
      let cid = db._create("UnitTestsReplication");
      let cuid = cid.properties()["globallyUniqueId"];
      let cid2 = db._createEdgeCollection("UnitTestsReplication2", { waitForSync: true });
      let cuid2 = cid2.properties()["globallyUniqueId"];

      let cmd = api + `/inventory?includeSystem=true&global=true&batchId=${batchId}`;
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      let all = doc.parsedBody;

      assertTrue(all.hasOwnProperty('state'));
      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      assertTrue(all.hasOwnProperty('databases'));
      let databases = all['databases'];

      let filtered = [ ];
      for (const [name, database] of Object.entries(databases)) {
        assertTrue(database.hasOwnProperty('collections'));

        let collections = database["collections"];
        filtered = [ ];
        collections.forEach(collection => {
          if ([ "UnitTestsReplication", "UnitTestsReplication2" ].includes(collection["parameters"]["name"])) {
            filtered.push(collection);
          }
          assertTrue(collection["parameters"].hasOwnProperty('globallyUniqueId'));
        });
      }
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
      assertEqual(typeof parameters["cid"],'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication");
      assertFalse(parameters["waitForSync"]);
      assertTrue(parameters.hasOwnProperty("globallyUniqueId"));
      assertEqual(parameters["globallyUniqueId"], cuid);

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
      assertEqual(typeof parameters["cid"],'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication2");
      assertTrue(parameters["waitForSync"]);
      assertTrue(parameters.hasOwnProperty("globallyUniqueId"));
      assertEqual(parameters["globallyUniqueId"], cuid2);

      assertEqual(c['indexes'], [ ]);
    },

    test_checks_the_inventory_with_indexes: function() {
      let cid = db._create("UnitTestsReplication");
      let cuid = cid.properties()["globallyUniqueId"];
      let cid2 = db._create("UnitTestsReplication2");
      let cuid2 = cid2.properties()["globallyUniqueId"];

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

      let cmd = api + `/inventory?batchId=${batchId}&global=true`;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);

      let all = doc.parsedBody;

      assertTrue(all.hasOwnProperty('state'));
      let state = all['state'];
      assertTrue(state['running']);
      assertMatch(/^[0-9]+$/, state['lastLogTick']);
      assertMatch(/^[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+Z$/, state['time']);

      let filtered = [ ];
      assertTrue(all.hasOwnProperty('databases'));
      let databases = all['databases'];
      for (const [name, database] of Object.entries(databases)) {
        assertTrue(database.hasOwnProperty('collections'));
        let collections = database['collections'];
        collections.forEach(collection => {
          if ([ "UnitTestsReplication", "UnitTestsReplication2" ].includes(collection["parameters"]["name"])) {
            filtered.push(collection);
          }
        });
      }

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
      assertEqual(typeof parameters["cid"],'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication");
      assertFalse(parameters["waitForSync"]);
      assertTrue(parameters.hasOwnProperty("globallyUniqueId"));
      assertEqual(parameters["globallyUniqueId"], cuid);

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
      assertEqual(typeof parameters["cid"],'string');
      assertFalse(parameters["deleted"]);
      assertEqual(parameters["name"], "UnitTestsReplication2");
      assertFalse(parameters["waitForSync"]);
      assertTrue(parameters.hasOwnProperty("globallyUniqueId"));
      assertEqual(parameters["globallyUniqueId"], cuid2);

      indexes = c['indexes'];
      assertEqual(indexes.length, 1);

      idx = indexes[0];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "skiplist");
      assertTrue(idx["unique"]);
      assertEqual(idx["fields"], [ "d" ]);

    }
  };
}

// TODO: add CRUD tests for updates;

jsunity.run(dealing_with_the_applierSuite);
jsunity.run(dealing_with_wal_access_apiSuite);
jsunity.run(dealing_with_the_initial_dumpSuite);
return jsunity.done();
