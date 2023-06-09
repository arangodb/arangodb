/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertNotEqual */

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
const jsunity = require("jsunity");

const database = "DumpTestDatabase";
const collectionNameA = "A";
//const collectionNameB = "B";

function fillCollection(col, num) {
  let documents = [];
  for (let i = 0; i < num; i++) {
    documents.push({value: i});
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

function apiCreateContext(server, options) {
  return arango.POST_RAW(`/_api/dump/start?dbserver=${server}`, options);
}

function apiDropContext(server, ctx) {
  return arango.DELETE_RAW(`/_api/dump/${ctx}?dbserver=${server}`, {});
}

function apiNext(server, ctx, batchId, lastBatch) {
  let url = `/_api/dump/next/${ctx}?dbserver=${server}&batchId=${batchId}`;
  if (lastBatch) {
    url += `&lastBatch=${lastBatch}`;
  }
  return arango.POST_RAW(url, {});
}

function createContext(server, options) {
  const response = apiCreateContext(server, options);
  assertEqual(response.code, 201);
  assertNotUndefined(response.headers["x-arango-dump-id"]);
  const id = response.headers["x-arango-dump-id"];

  return {
    id,
    read: function* () {
      for (let batchId = 0; ; batchId++) {
        const response = apiNext(server, id, batchId, batchId > 0 ? batchId - 1 : undefined);
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
      apiDropContext(server, id);
    },
  };
}


function DumpAPI() {

  let collection;
  let oldDatabase;

  return {
    setUpAll: function () {
      oldDatabase = db._name();
      db._createDatabase(database);
      db._useDatabase(database);

      collection = db._create(collectionNameA, {numberOfShards: 6, replicationFactor: 2});
      fillCollection(collection, 10000);

    },
    tearDownAll: function () {
      db._useDatabase(oldDatabase);
      db._dropDatabase(database);
    },

    testCreateFinishContext: function () {
      const servers = getShardsByServer(collection);
      for (const [server, shards] of Object.entries(servers)) {
        let response = apiCreateContext(server, {batchSize: 1024, parallelism: 5, prefetchCount: 5, shards});

        assertEqual(response.code, 201);
        assertNotUndefined(response.headers["x-arango-dump-id"]);
        const contextId = response.headers["x-arango-dump-id"];

        response = apiDropContext(server, contextId);
        assertEqual(response.code, 200);
        response = apiDropContext(server, contextId);
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

      let response1 = apiNext(server, ctx.id, 0);
      assertEqual(response1.code, 200);

      let response2 = apiNext(server, ctx.id, 0);
      assertEqual(response2.code, 200);
      // it should be the same batch
      assertEqual(response1.body, response2.body);
      ctx.drop();
    },

    testBatchUnknownContext: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];

      let response1 = apiNext(server, "DOES-NOT-EXIST", 0);
      assertEqual(response1.code, 404);
    },

    testContextTTL: function () {
      const servers = getShardsByServer(collection);
      const server = Object.keys(servers)[0];
      const ctx = createContext(server, {shards: servers[server], ttl: 1});
      internal.sleep(10);
      let response = apiNext(server, ctx.id, 0);
      assertEqual(response.code, 404);
    },
  };
}

jsunity.run(DumpAPI);
return jsunity.done();
