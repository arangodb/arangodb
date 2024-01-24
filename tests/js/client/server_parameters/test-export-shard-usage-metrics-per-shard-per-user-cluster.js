/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024, ArangoDB Inc, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// Copyright 2024, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jwtSecret = 'abc123';

if (getOptions === true) {
  return {
    'server.export-shard-usage-metrics': "enabled-per-shard-per-user",
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
  };
}

const jsunity = require('jsunity');
const db = require('@arangodb').db;
const { getDBServers } = require("@arangodb/test-helper");
const request = require("@arangodb/request");
const users = require("@arangodb/users");
const crypto = require('@arangodb/crypto');
  
const jwt = crypto.jwtEncode(jwtSecret, {
  "server_id": "ABCD",
  "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
}, 'HS256');

function testSuite() {
  const baseName = "UnitTestsCollection";

  let usersAdded = [];

  let dropAddedUsers = () => {
    usersAdded.forEach((name) => { 
      try {
        users.remove(name);
      } catch (err) {}
    });
  };

  let addUser = (name, database, type) => {
    try {
      if (!usersAdded.includes(name)) {
        users.save(name, "");
        usersAdded.push(name);
      }
      users.grantDatabase(name, database, type);
    } catch (err) {
    }
  };

  let connectWith = function(protocol, user, password) {
    let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
    arango.reconnect(endpoint, db._name(), user, password);
  };

  let getRawMetrics = function() {
    let lines = [];
    getDBServers().forEach((server) => {
      let res = request({ method: "GET", url: server.url + "/_admin/usage-metrics", auth: { bearer: jwt } });
      lines = lines.concat(res.body.split(/\n/).filter((l) => l.match(/^arangodb_collection_leader_(reads|writes)_total/)));
    });
    return lines;
  };

  let getParsedMetrics = function(database, collection, user) {
    let collections = collection;
    if (!Array.isArray(collections)) {
      collections = [ collections ];
    }
    let users = user;
    if (!Array.isArray(users)) {
      users = [ users ]; 
    }
    let metrics = getRawMetrics();
    let result = {};
    metrics.forEach((line) => {
      let matches = line.match(/^arangodb_collection_leader_(reads|writes)_total\s*\{(.*)?\}\s*(\d+)$/);
      if (!matches) {
        return;
      }
      let type = matches[1];
      let amount = parseInt(matches[3]);
      let labels = matches[2];

      let found = {};
      labels.split(',').forEach((label) => {
        let [key,value] = label.split('=');
        found[key] = value.replace(/"/g, '');
      });
      
      assertTrue(found.hasOwnProperty("shard"), found);
      assertTrue(found.hasOwnProperty("db"), found);
      assertTrue(found.hasOwnProperty("collection"), found);
      assertTrue(found.hasOwnProperty("user"), found);
      
      if (found["db"] !== database || !collections.includes(found["collection"]) || !users.includes(found["user"])) {
        return;
      }
      
      let user = found["user"];
      if (!result.hasOwnProperty(user)) {
        result[user] = {};
      }

      if (!result[user].hasOwnProperty(type)) {
        // reads/writes
        result[user][type] = {};
      }
      
      let shard = found["shard"];
      if (!result[user][type].hasOwnProperty(shard)) {
        result[user][type][shard] = 0;
      }

      result[user][type][shard] += amount;
    });

    return result;
  };

  return {
    setUp: function() {
      connectWith("tcp", "root", "");
      usersAdded = [];
    },

    tearDown: function() {
      connectWith("tcp", "root", "");
      dropAddedUsers();
      usersAdded = [];
    },
    
    testDoesNotPoluteNormalMetricsAPI : function () {
      const cn = baseName + "0";

      let c = db._create(cn);
      try {
        // must insert first to read something back
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        for (let i = 0; i < 10; ++i) {
          c.document("test" + i);
        }
        
        let lines = [];
        getDBServers().forEach((server) => {
          let res = request({ method: "GET", url: server.url + "/_admin/metrics" });
          lines = lines.concat(res.body.split(/\n/).filter((l) => l.match(/^arangodb_collection_leader_(reads|writes)_total/)));
        });
        assertEqual([], lines);
      } finally {
        db._drop(cn);
      }
    },
    
    testNoMetricsJustForCreatingCollection : function () {
      const cn = baseName + "1";
  
      addUser("foo", db._name(), "rw");
      connectWith("tcp", "foo", "");

      assertEqual("foo", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        let parsed = getParsedMetrics(db._name(), cn, "foo");
        assertEqual({}, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
   
    testHasMetricsWhenReadingFromCollectionSingleReads : function () {
      const cn = baseName + "2";
      
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to read something back
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        for (let i = 0; i < 10; ++i) {
          c.document("test" + i);
        }
        
        let parsed = getParsedMetrics(db._name(), cn, "bar");
        assertEqual({ "bar": { "writes": { [shards[0]] : 1 }, "reads": { [shards[0]] : 10 } } }, parsed);
     
        // different user => empty result
        parsed = getParsedMetrics(db._name(), cn, "foo");
        assertEqual({}, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenReadingFromCollectionBatchRead : function () {
      const cn = baseName + "3";
      
      addUser("foo", db._name(), "ro");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to read something back
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
       
        docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push("test" + i);
        }
        c.document(docs);
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 1 }, "reads": { [shards[0]] : 1 } } }, parsed);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        c.document(docs);

        parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "foo": { "reads": { [shards[0]] : 1 } }, "bar": { "writes": { [shards[0]] : 1 }, "reads": { [shards[0]] : 1 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleInserts : function () {
      const cn = baseName + "4";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        for (let i = 0; i < 10; ++i) {
          c.insert({ value: i });
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 10 } } }, parsed);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 20; ++i) {
          c.insert({ value: i });
        }

        parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 10 } }, "foo": { "writes": { [shards[0]] : 20 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchInsert : function () {
      const cn = baseName + "5";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ value: i });
        }
        c.insert(docs);
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 1 } } }, parsed);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        c.insert(docs);
        c.insert(docs);

        parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 1 } }, "foo": { "writes": { [shards[0]] : 2 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleRemoves : function () {
      const cn = baseName + "6";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to remove something later
        let docs = [];
        for (let i = 0; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        for (let i = 0; i < 5; ++i) {
          c.remove("test" + i);
        }
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        for (let i = 5; i < 20; ++i) {
          c.remove("test" + i);
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 6 } }, "foo": { "writes": { [shards[0]] : 15 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchRemove : function () {
      const cn = baseName + "7";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to remove something later
        let docs = [];
        for (let i = 0; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        docs = [];
        for (let i = 0; i < 5; ++i) {
          docs.push("test" + i);
        }
        c.remove(docs);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        docs = [];
        for (let i = 5; i < 20; ++i) {
          docs.push("test" + i);
        }
        c.remove(docs);
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 2 } }, "foo" : { "writes": { [shards[0]] : 1 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleUpdates : function () {
      const cn = baseName + "8";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to update something later
        let docs = [];
        for (let i = 0; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        for (let i = 0; i < 10; ++i) {
          c.update("test" + i, { value: i + 1 });
        }
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        for (let i = 10; i < 20; ++i) {
          c.update("test" + i, { value: i + 1 });
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 11 } }, "foo": { "writes" : { [shards[0]] : 10 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchUpdate : function () {
      const cn = baseName + "9";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to update something later
        let docs = [];
        for (let i = 0; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i + 1 });
        }
        c.update(docs, docs);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        docs = [];
        for (let i = 10; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i + 1 });
        }
        c.update(docs, docs);
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 2 } }, "foo": { "writes": { [shards[0]] : 1 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleReplaces : function () {
      const cn = baseName + "10";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to replace something later
        let docs = [];
        for (let i = 0; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        for (let i = 0; i < 10; ++i) {
          c.replace("test" + i, { value: i + 1 });
        }
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        for (let i = 10; i < 20; ++i) {
          c.replace("test" + i, { value: i + 1 });
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 11 } }, "foo" : { "writes" : { [shards[0]] : 10 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchReplace : function () {
      const cn = baseName + "11";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to replace something later
        let docs = [];
        for (let i = 0; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i + 1 });
        }
        c.replace(docs, docs);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        docs = [];
        for (let i = 10; i < 20; ++i) {
          docs.push({ _key: "test" + i, value: i + 1 });
        }
        c.replace(docs, docs);
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 2 } }, "foo": { "writes": { [shards[0]] : 1 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
   
    testHasMetricsWhenWritingIntoCollectionSingleInsertsMultiShard : function () {
      const cn = baseName + "12";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn, { numberOfShards: 3 });
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        for (let i = 0; i < 25; ++i) {
          c.insert({ value: i });
        }
        
        let counts1 = c.count(true);

        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 30; ++i) {
          c.insert({ value: i });
        }
       
        let counts2 = c.count(true);
        Object.keys(counts2).forEach((s) => {
          counts2[s] -= counts1[s];
        });

        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": counts1 }, "foo": { writes: counts2 } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQL : function () {
      const cn = baseName + "13";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        db._query("FOR doc IN " + cn + " RETURN doc");
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 5; ++i) {
          db._query("FOR doc IN " + cn + " RETURN doc");
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "reads": { [shards[0]] : 1 } }, "foo": { "reads": { [shards[0]] : 5 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQLMultiShard : function () {
      const cn = baseName + "14";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn, {numberOfShards: 3});
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        db._query("FOR doc IN " + cn + " RETURN doc");
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 6; ++i) {
          db._query("FOR doc IN " + cn + " RETURN doc");
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        let expected1 = {};
        let expected2 = {};
        shards.forEach((s) => { expected1[s] = 1; expected2[s] = 6; });
        assertEqual({ "bar": { "reads": expected1 }, "foo": { "reads": expected2 } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWriteAQL : function () {
      const cn = baseName + "15";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        db._query("FOR i IN 1..1000 INSERT {} INTO " + cn);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 3; ++i) {
          db._query("FOR i IN 1..100 INSERT {} INTO " + cn);
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 1 } }, "foo": { "writes": { [shards[0]] : 3 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWriteAQLMultiShard : function () {
      const cn = baseName + "16";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn, {numberOfShards: 3});
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        db._query("FOR i IN 1..1000 INSERT {} INTO " + cn);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 3; ++i) {
          db._query("FOR i IN 1..666 INSERT {} INTO " + cn);
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        let expected1 = {};
        let expected2 = {};
        shards.forEach((s) => { expected1[s] = 1; expected2[s] = 3; });
        assertEqual({ "bar": { "writes": expected1 }, "foo": { "writes": expected2 } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },

    testHasMetricsReadOnlyAQLMultiCollections : function () {
      const cn = baseName + "17";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c1 = db._create(cn);
      let c2 = db._create(cn + "_2");
      try {
        db._query("FOR doc1 IN " + c1.name() + " FOR doc2 IN " + c2.name() + " RETURN doc1");
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 10; ++i) {
          db._query("FOR doc1 IN " + c1.name() + " RETURN doc1");
        }
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()], ["foo", "bar"]);
        
        let expected1 = {};
        let expected2 = {};
        c1.shards().forEach((s) => { expected1[s] = 1; expected2[s] = 10; });
        c2.shards().forEach((s) => { expected1[s] = 1; });

        assertEqual({ "bar": { "reads": expected1 }, "foo": { "reads": expected2 } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsReadWriteAQLMultiCollections : function () {
      const cn = baseName + "18";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c1 = db._create(cn, { numberOfShards: 5 });
      let c2 = db._create(cn + "_2", { numberOfShards: 3 });
      try {
        db._query("FOR doc IN " + c1.name() + " INSERT {} INTO " + c2.name());
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 3; ++i) {
          db._query("FOR doc1 IN " + c1.name() + " FOR doc2 IN " + c2.name() + " RETURN doc1");
        }
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()], ["foo", "bar"]);
        
        let expected1 = { "reads": {}, "writes": {} };
        let expected2 = { "reads": {} };
        c1.shards().forEach((s) => { expected1["reads"][s] = 1; });
        c2.shards().forEach((s) => { expected1["writes"][s] = 1; });
        c1.shards().forEach((s) => { expected2["reads"][s] = 3; });
        c2.shards().forEach((s) => { expected2["reads"][s] = 3; });

        assertEqual({ "bar": expected1, "foo": expected2 }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsExclusiveAQLMultiCollections : function () {
      const cn = baseName + "19";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c1 = db._create(cn, { numberOfShards: 5 });
      let c2 = db._create(cn + "_2", { numberOfShards: 3 });
      try {
        db._query("FOR i IN 1..1000 INSERT {} INTO " + c1.name() + " OPTIONS {exclusive: true} INSERT {} INTO " + c2.name() +  " OPTIONS {exclusive: true}");
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 2; ++i) {
          db._query("FOR i IN 1..3 INSERT {} INTO " + c1.name() + " OPTIONS {exclusive: true} FOR doc IN " + c2.name() +  " RETURN doc");
        }

        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()], ["foo", "bar"]);
        
        let expected1 = { "writes": {} };
        let expected2 = { "reads": {}, "writes": {} };
        c1.shards().forEach((s) => { expected1["writes"][s] = 1; });
        c2.shards().forEach((s) => { expected1["writes"][s] = 1; });
        c1.shards().forEach((s) => { expected2["writes"][s] = 2; });
        c2.shards().forEach((s) => { expected2["reads"][s] = 2; });

        assertEqual({ "bar": expected1, "foo": expected2 }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleInsertsFollowerShards : function () {
      const cn = baseName + "20";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn, { replicationFactor: 2 });
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        for (let i = 0; i < 10; ++i) {
          c.insert({ value: i });
        }
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 23; ++i) {
          c.insert({ value: i });
        }
       
        // follower shards should not count here
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 10 } }, "foo": { "writes": { [shards[0]] : 23 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsAQLFollowerShards : function () {
      const cn = baseName + "21";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c1 = db._create(cn, { numberOfShards: 5, replicationFactor: 2 });
      let c2 = db._create(cn + "_2", { numberOfShards: 3, replicationFactor: 2 });
      try {
        db._query("FOR i IN 1..1000 INSERT {} INTO " + c1.name() + " OPTIONS {exclusive: true} INSERT {} INTO " + c2.name() +  " OPTIONS {exclusive: true}");
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 0; i < 2; ++i) {
          db._query("FOR i IN 1..3 INSERT {} INTO " + c1.name() + " OPTIONS {exclusive: true} FOR doc IN " + c2.name() +  " RETURN doc");
        }
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()], ["foo", "bar"]);
        
        // follower shards should not count here
        let expected1 = { "writes": {} };
        let expected2 = { "reads": {}, "writes": {} };
        c1.shards().forEach((s) => { expected1["writes"][s] = 1; });
        c2.shards().forEach((s) => { expected1["writes"][s] = 1; });
        c1.shards().forEach((s) => { expected2["writes"][s] = 2; });
        c2.shards().forEach((s) => { expected2["reads"][s] = 2; });

        assertEqual({ "bar": expected1, "foo": expected2 }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsMultipleDatabases : function () {
      const cn = baseName + "22";

      const databases = [baseName + "1", baseName + "2", baseName + "3"];

      try {
        databases.forEach((name) => {
          db._createDatabase(name);
          addUser("bar", db._name(), "rw");
          addUser("foo", db._name(), "rw");
        });

        databases.forEach((name) => {
          db._useDatabase(name);
      
          let c = db._create(cn);
          let shards = c.shards();
          assertEqual(1, shards.length);

          connectWith("tcp", "bar", "");
          for (let i = 0; i < 10; ++i) {
            c.insert({ _key: "test" + i, value: i });
          }
          
          connectWith("tcp", "foo", "");
          
          for (let i = 0; i < 100; ++i) {
            c.document("test" + (i % 10));
          }
        
          let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
          assertEqual({ "bar": { "writes": { [shards[0]] : 10 } }, "foo" : { "reads": { [shards[0]] : 100 } } }, parsed);
        });
      } finally {
        connectWith("tcp", "root", "");
        db._useDatabase("_system");
        databases.forEach((name) => {
          try {
            db._dropDatabase(name);
          } catch (err) {
          }
        });
      }
    },
    
    testHasMetricsWhenTruncating : function () {
      const cn = baseName + "23";

      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      assertEqual("bar", arango.connectedUser());

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        c.truncate();
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());

        for (let i = 0; i < 3; ++i) {
          c.truncate();
        }
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 1 } }, "foo": { "writes": { [shards[0]] : 3 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenPerformingMixedOperations : function () {
      const cn = baseName + "24";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        for (let i = 0; i < 10; ++i) {
          c.insert({ _key: "test" + i });
        }
        
        for (let i = 0; i < 5; ++i) {
          c.document("test" + i);
        }
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        for (let i = 1000; i < 1002; ++i) {
          c.exists("test" + i);
        }
        
        for (let i = 6; i < 10; ++i) {
          c.update("test" + i, { value: i + 1 });
        }
        
        connectWith("tcp", "bar", "");
        assertEqual("bar", arango.connectedUser());
        
        db._query("FOR doc IN " + cn + " INSERT {} INTO " + cn);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        db._query("FOR doc IN " + cn + " RETURN doc");

        c.truncate();
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": { [shards[0]] : 11 }, "reads": { [shards[0]] : 5 } }, "foo": { "writes": { [shards[0]] : 5 }, "reads": { [shards[0]] : 3 } } }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },

    testHasMetricsWhenUsingCustomShardKey : function () {
      const cn = baseName + "25";
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      addUser("baz", db._name(), "ro");
      connectWith("tcp", "bar", "");

      let c = db._create(cn, {shardKeys: ["qux"], numberOfShards: 3});
      try {
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ qux: "test" + i });
        }
        let keys = c.insert(docs).map((d) => d._key);
        
        let parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        let expected = { "bar": { "writes": {} } };
        c.shards().forEach((s) => {
          expected["bar"]["writes"][s] = 1;
        });
        assertEqual(expected, parsed);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());
        
        keys.forEach((k, i) => {
          c.update(k, { value: i });
        });
        
        parsed = getParsedMetrics(db._name(), cn, ["foo", "bar"]);
        expected["foo"] = { "writes": {} };
        c.shards().forEach((s) => {
          expected["foo"]["writes"][s] = 10;
        });
        assertEqual(expected, parsed);
        
        connectWith("tcp", "baz", "");
        assertEqual("baz", arango.connectedUser());
        
        keys.forEach((k, i) => {
          c.document(k);
        });
        
        parsed = getParsedMetrics(db._name(), cn, ["baz"]);
        expected = {"reads": {} };
        c.shards().forEach((s) => {
          expected["reads"][s] = 10;
        });
        assertEqual({ "baz": expected }, parsed);
      } finally {
        connectWith("tcp", "root", "");
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenInsertIntoSmartGraph : function () {
      const isEnterprise = require("internal").isEnterprise();
      if (!isEnterprise) {
        return;
      }

      const vn = baseName + "Vertices26";
      const en = baseName + "Edges26";
      const gn = "UnitTestsGraph";
      const graphs = require("@arangodb/smart-graph");
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      let cleanup = function () {
        try {
          graphs._drop(gn, true);
        } catch (err) {}
        db._drop(vn);
        db._drop(en);
      };
      
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor: 2, smartGraphAttribute: "testi" });
      try {
        for (let i = 0; i < 25; ++i) {
          db[vn].insert({ _key: "test" + (i % 10) + ":test" + i, testi: "test" + (i % 10) });
        }

        let counts = db[vn].count(true);
        let parsed = getParsedMetrics(db._name(), vn, ["foo", "bar"]);
        assertEqual({ "bar": { "writes": counts } }, parsed);

        let keys = [];
        for (let i = 0; i < 20; ++i) {
          keys.push(db[en].insert({ _from: vn + "/test" + i + ":test" + (i % 10), _to: vn + "/test" + ((i + 1) % 100) + ":test" + (i % 10), testi: (i % 10) })._key);
        }
        
        keys.push(db[en].insert({ _from: vn + "/test0:test0", _to: vn + "/testmann-does-not-exist:test0", testi: "test0" })._key);
        
        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en], ["foo", "bar"]);

        // edges are only counted for the _from_ shard here
        let expected = { "bar": { "writes": {} } };
        counts = db["_from_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["bar"]["writes"][k] = counts[k];
        });
        counts = db["_to_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["bar"]["writes"][k] = counts[k];
        });
        assertEqual(expected, parsed);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());

        keys.forEach((key) => {
          db[en].document(key);
        });

        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en], ["foo", "bar"]);

        assertEqual(expected.writes, parsed.writes);
        // reads should sum up to 21 in total
        let sum = 0;
        Object.keys(parsed["foo"]["reads"]).forEach((s) => {
          sum += parsed["foo"]["reads"][s];
        });
        assertEqual(21, sum);
      } finally {
        connectWith("tcp", "root", "");
        cleanup();
      }
    },
    
    testHasMetricsWhenAQLingSmartGraph : function () {
      const isEnterprise = require("internal").isEnterprise();
      if (!isEnterprise) {
        return;
      }
  
      const vn = baseName + "Vertices27";
      const en = baseName + "Edges27";
      const gn = "UnitTestsGraph";
      const graphs = require("@arangodb/smart-graph");
      
      addUser("foo", db._name(), "rw");
      addUser("bar", db._name(), "rw");
      connectWith("tcp", "bar", "");

      let cleanup = function () {
        try {
          graphs._drop(gn, true);
        } catch (err) {}
        db._drop(vn);
        db._drop(en);
      };
      
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor: 2, smartGraphAttribute: "testi" });
      try {
        db._query("FOR i IN 0..24 INSERT {_key: CONCAT('test', (i % 10), ':test', i), testi: CONCAT('test', (i % 10))} INTO " + vn);
        
        let parsed = getParsedMetrics(db._name(), vn, ["foo", "bar"]);
        let expected = { "bar": { "writes": {} } };
        db[vn].shards().forEach((s) => {
          expected["bar"]["writes"][s] = 1;
        });
        assertEqual(expected, parsed);

        let keys = db._query("FOR i IN 0..19 INSERT {_from: CONCAT('" + vn + "/test', i, ':test', (i % 10)), _to: CONCAT('" + vn + "/test', ((i + 1) % 100), ':test', (i % 10)), testi: (i % 10)} INTO " + en + " RETURN NEW._key").toArray();
        
        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en], ["foo", "bar"]);

        expected = { "bar": { "writes": {} } };
        let counts = db["_from_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["bar"]["writes"][k] = 1;
        });
        counts = db["_to_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["bar"]["writes"][k] = 1;
        });
        counts = db["_local_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["bar"]["writes"][k] = 1;
        });

        assertEqual(expected, parsed);
        
        connectWith("tcp", "foo", "");
        assertEqual("foo", arango.connectedUser());

        db._query("FOR doc IN " + en + " FILTER doc._key IN @keys RETURN doc", { keys });

        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en], ["foo", "bar"]);

        assertEqual(expected["bar"], parsed["bar"]);
        // reads should sum up to 20 in total
        let sum = 0;
        Object.keys(parsed["foo"]["reads"]).forEach((s) => {
          sum += parsed["foo"]["reads"][s];
        });
        assertEqual(db["_from_" + en].shards().length + db["_to_" + en].shards().length, sum);
      } finally {
        connectWith("tcp", "root", "");
        cleanup();
      }
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
