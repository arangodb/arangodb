/*jshint globalstrict:false, strict:false */
/* global getOptions, assertNotEqual, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.export-shard-usage-metrics': "enabled-per-shard",
    'server.usage-tracking-include-system-collections': "false",
  };
}

const jsunity = require('jsunity');
const db = require('@arangodb').db;
const { getDBServers, getCoordinators } = require("@arangodb/test-helper");
const request = require("@arangodb/request");

function testSuite() {
  const baseName = "UnitTestsCollection";

  let getRawMetrics = function() {
    let lines = [];
    getDBServers().forEach((server) => {
      let res = request({ method: "GET", url: server.url + "/_admin/metrics" });
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
    testNoMetricsJustForCreatingCollection : function () {
      const cn = baseName + "1";

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
      const cn = baseName + "2";

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
      const cn = baseName + "3";

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
      const cn = baseName + "4";

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
      const cn = baseName + "5";

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
      const cn = baseName + "6";

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
      const cn = baseName + "7";

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
      const cn = baseName + "8";

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
      const cn = baseName + "9";

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
      const cn = baseName + "10";

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
      const cn = baseName + "11";

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
      const cn = baseName + "12";

      let c = db._create(cn, { numberOfShards: 3 });
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        for (let i = 0; i < 20; ++i) {
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
      const cn = baseName + "13";

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
      const cn = baseName + "14";

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
      const cn = baseName + "15";

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
      const cn = baseName + "16";

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

    testHasMetricsReadOnlyMultiCollectionsAQL : function () {
      const cn = baseName + "17";

      let c1 = db._create(cn);
      let c2 = db._create(cn + "_2");
      try {
        db._query("FOR doc1 IN " + c1.name() + " FOR doc2 IN " + c2.name() + " RETURN doc1");
        
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
        // multiple shards
        // follower metrics?
        // AQL

  };
}

jsunity.run(testSuite);
return jsunity.done();
