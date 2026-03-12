/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotMatch, assertNotEqual, assertNotUndefined, assertMatch */

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

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");


let api = "/_api/replication";

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

      let body = { "type" : "persistent", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW("/_api/index?collection=UnitTestsReplication", body);
      assertEqual(doc.code, 201);

      body = { "type" : "persistent", "unique" : false, "fields" : [ "c" ] };
      doc = arango.POST_RAW("/_api/index?collection=UnitTestsReplication", body);
      assertEqual(doc.code, 201);

      // create indexes for second collection;
      body = { "type" : "persistent", "unique" : true, "fields" : [ "d" ] };
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
      assertEqual(idx["type"], "persistent");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "a", "b" ]);

      idx = indexes[1];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "persistent");
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
      assertEqual(idx["type"], "persistent");
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

////////////////////////////////////////////////////////////////////////////////;
// logger;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_the_logger_Suite () {
  return {
    setUp: function() {
      db._drop("UnitTestsReplication");
      let res = db._createDatabase("UnitTestDB");;
      assertTrue(res);
    },

    tearDown: function() {
      let res = db._dropDatabase("UnitTestDB");;
      assertTrue(res);
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

      let body = { "type" : "persistent", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication`, body);
      assertEqual(doc.code, 201);

      body = { "type" : "persistent", "unique" : false, "fields" : [ "c" ] };
      doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication`, body);
      assertEqual(doc.code, 201);

      // create indexes for second collection;
      body = { "type" : "geo", "unique" : false, "fields" : [ "a", "b" ] };
      doc = arango.POST_RAW(`${idxUrl}?collection=UnitTestsReplication2`, body);
      assertEqual(doc.code, 201);

      body = { "type" : "persistent", "unique" : true, "fields" : [ "d" ] };
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
      assertEqual(idx["type"], "persistent");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "a", "b" ]);

      idx = indexes[1];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "persistent");
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
      assertEqual(indexes.length, 2);

      idx = indexes[0];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "geo");
      assertFalse(idx["unique"]);
      assertEqual(idx["fields"], [ "a", "b" ]);

      idx = indexes[1];
      assertMatch(/^[0-9]+$/, idx["id"]);
      assertEqual(idx["type"], "persistent");
      assertTrue(idx["unique"]);
      assertEqual(idx["fields"], [ "d" ]);
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

jsunity.run(dealing_with_the_loggerSuite);
jsunity.run(dealing_with_batches_and_logger_stateSuite);
jsunity.run(dealing_with_the_initial_dump_interfaceSuite);

jsunity.run(dealing_with_the_logger_Suite);
jsunity.run(dealing_with_the_initial_dumSuite);
return jsunity.done();
