/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue */

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
/// @author Copyright 2024, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.export-shard-usage-metrics': "enabled-per-shard",
  };
}

const jsunity = require('jsunity');
const db = require('@arangodb').db;
const { getDBServers, getCoordinators } = require("@arangodb/test-helper");
const request = require("@arangodb/request");

function testSuite() {
  const baseName = "UnitTestsCollection";
  
  let nextCollectionId = 0;

  let getUniqueCollectionName = () => {
    return baseName + nextCollectionId++;
  };

  let getRawMetrics = function() {
    let lines = [];
    getDBServers().forEach((server) => {
      let res = request({ method: "GET", url: server.url + "/_admin/usage-metrics" });
      lines = lines.concat(res.body.split(/\n/).filter((l) => l.match(/^arangodb_collection_leader_(reads|writes)_total/)));
    });
    return lines;
  };

  let getParsedMetrics = function(database, collection) {
    let collections = collection;
    if (!Array.isArray(collections)) {
      collections = [ collections ];
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
      
      if (found["db"] !== database || !collections.includes(found["collection"])) {
        return;
      }

      if (!result.hasOwnProperty(type)) {
        // reads/writes
        result[type] = {};
      }
      
      let shard = found["shard"];
      if (!result[type].hasOwnProperty(shard)) {
        result[type][shard] = 0;
      }
      result[type][shard] += amount;
    });

    return result;
  };

  return {
    testDoesNotPoluteNormalMetricsAPI : function () {
      const cn = getUniqueCollectionName();

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
        
        // check if the normal metrics endpoint exports any shard-specific metrics
        let lines = [];
        getDBServers().forEach((server) => {
          let res = request({ method: "GET", url: server.url + "/_admin/metrics" });
          lines = lines.concat(res.body.split(/\n/).filter((l) => l.match(/^arangodb_collection_leader_(reads|writes)_total/)));
        });
        assertEqual([], lines);

        // check if the usage-metrics endpoint exports any regular metrics
        lines = [];
        getDBServers().forEach((server) => {
          let res = request({ method: "GET", url: server.url + "/_admin/usage-metrics" });
          // we look for any metric name starting with "rocksdb_" here as a placeholder
          lines = lines.concat(res.body.split(/\n/).filter((l) => l.match(/^rocksdb_/)));
        });
        assertEqual([], lines);
      } finally {
        db._drop(cn);
      }
    },

    testNoMetricsJustForCreatingCollection : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({}, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenReadingFromCollectionSingleReads : function () {
      const cn = getUniqueCollectionName();

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
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 1 }, "reads": { [shards[0]] : 10 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenReadingFromCollectionBatchRead : function () {
      const cn = getUniqueCollectionName();

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
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 1 }, "reads": { [shards[0]] : 1 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleInserts : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        for (let i = 0; i < 10; ++i) {
          c.insert({ value: i });
        }
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 10 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchInsert : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ value: i });
        }
        c.insert(docs);
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 1 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleRemoves : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to remove something later
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        for (let i = 0; i < 10; ++i) {
          c.remove("test" + i);
        }
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 11 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchRemove : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to remove something later
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        c.remove(docs);
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 2 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleUpdates : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to update something later
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        for (let i = 0; i < 10; ++i) {
          c.update("test" + i, { value: i + 1 });
        }
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 11 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchUpdate : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to update something later
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i + 1 });
        }
        c.update(docs, docs);
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 2 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleReplaces : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to replace something later
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        for (let i = 0; i < 10; ++i) {
          c.replace("test" + i, { value: i + 1 });
        }
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 11 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchReplace : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        // must insert first to replace something later
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);

        docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ _key: "test" + i, value: i + 1 });
        }
        c.replace(docs, docs);
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 2 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleInsertsMultiShard : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn, { numberOfShards: 3 });
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        for (let i = 0; i < 35; ++i) {
          c.insert({ value: i });
        }
       
        let counts = c.count(true);

        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": counts }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQL : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        db._query("FOR doc IN " + cn + " RETURN doc");
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "reads": { [shards[0]] : 1 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQLMultiShard : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn, {numberOfShards: 3});
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        db._query("FOR doc IN " + cn + " RETURN doc");
        
        let parsed = getParsedMetrics(db._name(), cn);
        let expected = {};
        shards.forEach((s) => { expected[s] = 1; });
        assertEqual({ "reads": expected }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWriteAQL : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        db._query("FOR i IN 1..1000 INSERT {} INTO " + cn);
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 1 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWriteAQLMultiShard : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn, {numberOfShards: 3});
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        db._query("FOR i IN 1..1000 INSERT {} INTO " + cn);
        
        let parsed = getParsedMetrics(db._name(), cn);
        let expected = {};
        shards.forEach((s) => { expected[s] = 1; });
        assertEqual({ "writes": expected }, parsed);
      } finally {
        db._drop(cn);
      }
    },

    testHasMetricsReadOnlyAQLMultiCollections : function () {
      let c1 = db._create(getUniqueCollectionName());
      let c2 = db._create(getUniqueCollectionName());
      try {
        db._query(`FOR doc1 IN ${c1.name()} FOR doc2 IN ${c2.name()} RETURN doc1`);
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        
        let expected = {};
        c1.shards().forEach((s) => { expected[s] = 1; });
        c2.shards().forEach((s) => { expected[s] = 1; });

        assertEqual({ "reads": expected }, parsed);
      } finally {
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsReadWriteAQLMultiCollections : function () {
      let c1 = db._create(getUniqueCollectionName(), { numberOfShards: 5 });
      let c2 = db._create(getUniqueCollectionName(), { numberOfShards: 3 });
      try {
        db._query(`FOR doc IN ${c1.name()} INSERT {} INTO ${c2.name()}`);
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        
        let expected = { "reads": {}, "writes": {} };
        c1.shards().forEach((s) => { expected["reads"][s] = 1; });
        c2.shards().forEach((s) => { expected["writes"][s] = 1; });

        assertEqual(expected, parsed);
      } finally {
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsExclusiveAQLMultiCollections : function () {
      let c1 = db._create(getUniqueCollectionName(), { numberOfShards: 5 });
      let c2 = db._create(getUniqueCollectionName(), { numberOfShards: 3 });
      try {
        db._query(`FOR i IN 1..1000 INSERT {} INTO ${c1.name()} OPTIONS {exclusive: true} INSERT {} INTO ${c2.name()} OPTIONS {exclusive: true}`);
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        
        let expected = { "writes": {} };
        c1.shards().forEach((s) => { expected["writes"][s] = 1; });
        c2.shards().forEach((s) => { expected["writes"][s] = 1; });

        assertEqual(expected, parsed);
      } finally {
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleInsertsFollowerShards : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn, { replicationFactor: 2 });
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        for (let i = 0; i < 10; ++i) {
          c.insert({ value: i });
        }
       
        // follower shards should not count here
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 10 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsAQLFollowerShards : function () {
      let c1 = db._create(getUniqueCollectionName(), { numberOfShards: 5, replicationFactor: 2 });
      let c2 = db._create(getUniqueCollectionName(), { numberOfShards: 3, replicationFactor: 2 });
      try {
        db._query(`FOR i IN 1..1000 INSERT {} INTO ${c1.name()} OPTIONS {exclusive: true} INSERT {} INTO ${c2.name()} OPTIONS {exclusive: true}`);
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        
        // follower shards should not count here
        let expected = { "writes": {} };
        c1.shards().forEach((s) => { expected["writes"][s] = 1; });
        c2.shards().forEach((s) => { expected["writes"][s] = 1; });

        assertEqual(expected, parsed);
      } finally {
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsMultipleDatabases : function () {
      const cn = getUniqueCollectionName();

      const databases = [baseName + "1", baseName + "2", baseName + "3"];

      try {
        databases.forEach((name) => {
          db._createDatabase(name);
        });

        databases.forEach((name) => {
          db._useDatabase(name);
      
          let c = db._create(cn);
          let shards = c.shards();
          assertEqual(1, shards.length);

          for (let i = 0; i < 10; ++i) {
            c.insert({ value: i });
          }
        
          let parsed = getParsedMetrics(db._name(), cn);
          assertEqual({ "writes": { [shards[0]] : 10 } }, parsed);
        });
      } finally {
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
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        c.truncate();
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 1 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenPerformingMixedOperations : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        for (let i = 0; i < 10; ++i) {
          c.insert({ _key: "test" + i });
        }
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 10 } }, parsed);
        
        for (let i = 0; i < 5; ++i) {
          c.document("test" + i);
        }
        
        for (let i = 1000; i < 1002; ++i) {
          c.exists("test" + i);
        }
        
        parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 10 }, "reads": { [shards[0]] : 7 } }, parsed);
        
        for (let i = 6; i < 10; ++i) {
          c.update("test" + i, { value: i + 1 });
        }
        
        parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 14 }, "reads": { [shards[0]] : 7 } }, parsed);

        db._query("FOR doc IN " + cn + " INSERT {} INTO " + cn);
        
        parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 15 }, "reads": { [shards[0]] : 7 } }, parsed);
        
        db._query("FOR doc IN " + cn + " RETURN doc");

        parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 15 }, "reads": { [shards[0]] : 8 } }, parsed);

        c.truncate();
        
        parsed = getParsedMetrics(db._name(), cn);
        assertEqual({ "writes": { [shards[0]] : 16 }, "reads": { [shards[0]] : 8 } }, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenUsingCustomShardKey : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn, {shardKeys: ["qux"], numberOfShards: 3});
      try {
        let docs = [];
        for (let i = 0; i < 10; ++i) {
          docs.push({ qux: "test" + i });
        }
        let keys = c.insert(docs).map((d) => d._key);
        
        let parsed = getParsedMetrics(db._name(), cn);
        let expected = {};
        c.shards().forEach((s) => {
          expected[s] = 1;
        });
        assertEqual({ "writes": expected }, parsed);
        
        keys.forEach((k, i) => {
          c.update(k, { value: i });
        });
        
        parsed = getParsedMetrics(db._name(), cn);
        expected = {};
        c.shards().forEach((s) => {
          expected[s] = 11;
        });
        assertEqual({ "writes": expected }, parsed);
        
        keys.forEach((k, i) => {
          c.document(k);
        });
        
        parsed = getParsedMetrics(db._name(), cn);
        expected = {"reads": {}, "writes": {}};
        c.shards().forEach((s) => {
          expected["writes"][s] = 11;
          expected["reads"][s] = 10;
        });
        assertEqual(expected, parsed);
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWhenInsertIntoSmartGraph : function () {
      const isEnterprise = require("internal").isEnterprise();
      if (!isEnterprise) {
        return;
      }

      const vn = getUniqueCollectionName();
      const en = getUniqueCollectionName();
      const gn = "UnitTestsGraph";
      const graphs = require("@arangodb/smart-graph");

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
        let parsed = getParsedMetrics(db._name(), vn);
        assertEqual({ "writes": counts }, parsed);

        let keys = [];
        for (let i = 0; i < 20; ++i) {
          keys.push(db[en].insert({ _from: vn + "/test" + i + ":test" + (i % 10), _to: vn + "/test" + ((i + 1) % 100) + ":test" + (i % 10), testi: (i % 10) })._key);
        }
        keys.push(db[en].insert({ _from: vn + "/test0:test0", _to: vn + "/testmann-does-not-exist:test0", testi: "test0" })._key);
        
        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);

        let expected = { "writes": {} };
        counts = db["_from_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["writes"][k] = counts[k];
        });
        counts = db["_to_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["writes"][k] = counts[k];
        });

        assertEqual(expected, parsed);

        keys.forEach((key) => {
          db[en].document(key);
        });

        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);

        assertEqual(expected.writes, parsed.writes);
        // reads should sum up to 21 in total
        let sum = 0;
        Object.keys(parsed.reads).forEach((s) => {
          sum += parsed.reads[s];
        });
        assertEqual(21, sum);
      } finally {
        cleanup();
      }
    },
    
    testHasMetricsWhenAQLingSmartGraph : function () {
      const isEnterprise = require("internal").isEnterprise();
      if (!isEnterprise) {
        return;
      }
  
      const vn = getUniqueCollectionName();
      const en = getUniqueCollectionName();
      const gn = "UnitTestsGraph";
      const graphs = require("@arangodb/smart-graph");

      let cleanup = function () {
        try {
          graphs._drop(gn, true);
        } catch (err) {}
        db._drop(vn);
        db._drop(en);
      };
      
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor: 2, smartGraphAttribute: "testi" });
      try {
        // smart vertex collection
        db._query(`FOR i IN 0..24 INSERT {_key: CONCAT('test', (i % 10), ':test', i), testi: CONCAT('test', (i % 10))} INTO ${vn}`);
        
        let parsed = getParsedMetrics(db._name(), vn);
        let expected = { "writes": {} };
        db[vn].shards().forEach((s) => {
          expected["writes"][s] = 1;
        });
        assertEqual(expected, parsed);

        // smart edge collection
        let keys = db._query(`FOR i IN 0..19 INSERT {_from: CONCAT('${vn}/test', i, ':test', (i % 10)), _to: CONCAT('${vn}/test', ((i + 1) % 100), ':test', (i % 10)), testi: (i % 10)} INTO ${en} RETURN NEW._key`).toArray();
        
        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);

        expected = { "writes": {} };
        let counts = db["_from_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["writes"][k] = 1;
        });
        counts = db["_to_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["writes"][k] = 1;
        });
        counts = db["_local_" + en].count(true);
        Object.keys(counts).forEach((k) => {
          expected["writes"][k] = 1;
        });

        assertEqual(expected, parsed);

        db._query(`FOR doc IN ${en} FILTER doc._key IN @keys RETURN doc`, { keys });

        parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);

        assertEqual(expected.writes, parsed.writes);
        // reads should sum up to 20 in total
        let sum = 0;
        Object.keys(parsed.reads).forEach((s) => {
          sum += parsed.reads[s];
        });
        assertEqual(db["_from_" + en].shards().length + db["_to_" + en].shards().length, sum);
      } finally {
        cleanup();
      }
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
