/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertNotEqual */

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
// / @author Lars Maier
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const jsunity = require("jsunity");
const dump = require('@arangodb/arango-dump');

const database = "DumpTestDatabase";
const collectionNameA = "A";
//const collectionNameB = "B";

function fillCollection(col, num) {
  let documents = [];
  for (let i = 0; i < num; i++) {
    documents.push({value: i, some: {nested:{value: 1234}}, flag: i % 3});
    if (documents.length === 1000) {
      col.save(documents);
      documents = [];
    }
  }
  if (documents.length > 0) {
    col.save(documents);
  }
}

function getShardsByServer(col) {
  const shards = col.shards(true);
  const servers = {};

  for (const s of Object.keys(shards)) {
    const leader = shards[s][0];
    if (servers[leader]) {
      servers[leader].push(s);
    } else {
      servers[leader] = [s];
    }
  }

  return servers;
}

function DumpAPI() {

  let collection;
  let oldDatabase;

  let contexts = [];
  
  function createContext(server, options) {
    const response = dump.start(options, server);
    assertEqual(response.code, 201, JSON.stringify(response));
    assertNotUndefined(response.headers["x-arango-dump-id"]);
    const id = response.headers["x-arango-dump-id"];
    contexts.push({server, id});

    return {
      id,
      read: function* () {
        for (let batchId = 0; ; batchId++) {
          const response = dump.next(id, batchId, batchId > 0 ? batchId-1: undefined, server);
          if (response.code === 204) {
            break;
          }
          assertEqual(response.code, 200);
          assertEqual(response.headers["content-type"], "application/x-arango-dump");

          const shard = response.headers["x-arango-dump-shard-id"];
          assertNotUndefined(shard);
          assertNotEqual(options.shards.indexOf(shard), -1);

          const jsonl = response.body.toString().split("\n");
          for (const line of jsonl) {
            if (line.length === 0) {
              continue;
            }
            yield [JSON.parse(line), shard];
          }
        }
      },
      drop: function () {
        contexts = contexts.filter(ctx => ctx.server !== server || ctx.id !== id);
        dump.delete(id, server);
      },
    };
  }

  return {
    setUpAll: function () {
      oldDatabase = db._name();
      db._createDatabase(database);
      db._useDatabase(database);

      collection = db._create(collectionNameA, {numberOfShards: 6, replicationFactor: 2});
      fillCollection(collection, 10000);
    },

    tearDown: function() {
      for (const ctx of contexts) {
        dump.delete(ctx.id, ctx.server);
      }
    },

    tearDownAll: function () {
      db._useDatabase(oldDatabase);
      db._dropDatabase(database);
    },

    testCreateFinishContext: function () {
      const servers = getShardsByServer(collection);
      for (const [server, shards] of Object.entries(servers)) {
        let response = dump.start({batchSize: 1024, parallelism: 5, prefetchCount: 5, shards}, server);

        assertEqual(response.code, 201);
        assertNotUndefined(response.headers["x-arango-dump-id"]);
        const contextId = response.headers["x-arango-dump-id"];

        response = dump.delete(contextId, server);
        assertEqual(response.code, 200);
        response = dump.delete(contextId, server);
        assertEqual(response.code, 404);
      }
    },

    testDumpServers: function () {
      const servers = getShardsByServer(collection);

      const docs = {};
      for (const [server, shards] of Object.entries(servers)) {
        const ctx = createContext(server, {shards});
        for (const [doc, shard] of ctx.read()) {
          assertNotEqual(shards.indexOf(shard), -1);
          docs[doc.value] = doc;
        }
      }

      assertEqual(Object.keys(docs).length, 10000);
      for (let i = 0; i < 10000; i++) {
        assertEqual(docs[i].value, i);
      }
    },

    testBatchRetry: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server]});

      let response1 = dump.next(ctx.id, 0, undefined, server);
      assertEqual(response1.code, 200);

      let response2 = dump.next(ctx.id, 0, undefined, server);
      assertEqual(response2.code, 200);
      // it should be the same batch
      assertEqual(response1.body, response2.body);
      ctx.drop();
    },

    testBatchUnknownContext: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];

      let response1 = dump.next("DOES-NOT-EXIST", 0, undefined, server);
      assertEqual(response1.code, 404);
    },

    testSimpleProjections: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server], projections: {"foo": ["value"]}});

      for (const [doc, shard] of ctx.read()) {
        assertEqual(Object.keys(doc), ["foo"]);
        assertTrue(typeof doc.foo === 'number');
      }
      ctx.drop();
    },

    testSimpleProjectionsNonExistentPath: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server], projections: {"foo": ["value"],
          "unknown": ["some", "non-existent", "path"]}});

      for (const [doc, shard] of ctx.read()) {
        assertEqual(Object.keys(doc), ["foo"]);
        assertTrue(typeof doc.foo === 'number');
        assertEqual(doc.unknown, undefined);
      }
      ctx.drop();
    },

    testProjectionsId: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server], projections: {"foo": ["_id"]}});

      for (const [doc, shard] of ctx.read()) {
        assertEqual(Object.keys(doc), ["foo"]);
        assertTrue(typeof doc.foo === 'string');
        assertTrue(/^A\/\d+$/.test(doc.foo));
      }
      ctx.drop();
    },

    testProjectionsNested: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server], projections: {"foo": ["some", "nested", "value"], "bar": ["value"]}});

      for (const [doc, shard] of ctx.read()) {
        assertEqual(Object.keys(doc).sort(), ["bar", "foo"]);
        assertEqual(doc.foo, 1234);
        assertTrue(typeof doc.bar === 'number');
      }
      ctx.drop();
    },

    testSimpleFilters: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server],
                                         filters: {type: "simple",
                                                   conditions: [{attributePath: ["flag"], value: 0}]}});

      for (const [doc, shard] of ctx.read()) {
        assertEqual(doc.flag, 0);
      }
      ctx.drop();
    },

    testNestedFilters: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server],
                                         filters: {type: "simple",
                                                   conditions: [{attributePath: ["flag"], value: 1},
                                                                {attributePath: ["some", "nested", "value"], value: 1234}]}});

      for (const [doc, shard] of ctx.read()) {
        assertEqual(doc.flag, 1);
        assertEqual(doc.some.nested.value, 1234);
      }
      ctx.drop();
    },

    testFilterWithObjectVal: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server],
                                         filters: {type: "simple",
                                                   conditions: [{attributePath: ["some"], value: {"nested": {"value": 1234}}}]}});

      for (const [doc, shard] of ctx.read()) {
        assertEqual(doc.some, {nested: {value: 1234}}, JSON.stringify(doc.some));
      }
      ctx.drop();
    },

    testContextTTL: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server], ttl: 1});
      internal.sleep(10);
      let response = dump.next(ctx.id, 0, undefined, server);
      assertEqual(response.code, 404);
    },
  };
}

jsunity.run(DumpAPI);
return jsunity.done();
