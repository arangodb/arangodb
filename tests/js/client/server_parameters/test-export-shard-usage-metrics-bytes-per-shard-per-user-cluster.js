/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertFalse, arango */

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
const internal = require('internal');
const { getDBServers, deriveTestSuite } = require("@arangodb/test-helper");
const request = require("@arangodb/request");
const users = require("@arangodb/users");
const crypto = require('@arangodb/crypto');

const jwt = crypto.jwtEncode(jwtSecret, {
  "server_id": "ABCD",
  "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
}, 'HS256');

// note: these tests will currently partially fail under replication2.
// the reason is that the tests expect the bytes_written metrics to be
// increased on followers, too.
// this is actually the case with replication1, as replication requests
// are sent to the followers along with all relevant information, so that
// when the request is handled on the followers, the metrics can be 
// increased normally.
// with replication2 however, the leader does not send replication2 requests
// to followers that result in normal document write operations. instead,
// the replication requests from the leader are first written to the
// replication log on the follower, and only eventually applied there.
// when the replication log writes are applied on the follower, the 
// information about which user initiated the operation is already lost.
// this can be fixed by storing the user information in the replicated
// log, and using it when the write operation is later applied on the
// follower.
function BaseTestSuite(targetUser) {
  const baseName = "UnitTestsCollection";
  let nextCollectionId = 0;

  let getUniqueCollectionName = () => {
    return baseName + nextCollectionId++;
  };

  let getRawMetrics = function() {
    let lines = [];
    getDBServers().forEach((server) => {
      let res = request({ method: "GET", url: server.url + "/_admin/usage-metrics", auth: { bearer: jwt } });
      assertEqual(200, res.status);
      lines = lines.concat(res.body.split(/\n/).filter((l) => l.match(/^arangodb_collection_requests_bytes_(read|written)_total/)));
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
      let matches = line.match(/^arangodb_collection_requests_bytes_(read|written)_total\s*\{(.*)?\}\s*(\d+)$/);
      if (!matches) {
        return;
      }
      let type = matches[1];
      // translate type used in metric name (read|written) to reads|written
      if (type === 'read') {
        type = 'reads';
      } else {
        assertEqual('written', type);
        type = 'writes';
      }
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
  
      if (found["db"] !== database || !collections.includes(found["collection"])) {
        return;
      }

      if (found["user"] !== targetUser) {
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
    setUpAll : function () {
      // set this failure point so that metrics updates are pushed immediately
      internal.debugSetFailAt("alwaysPublishShardMetrics");
    },
      
    tearDownAll : function () {
      internal.debugRemoveFailAt("alwaysPublishShardMetrics");
    },

    testDoesNotPolluteNormalMetricsAPI : function () {
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
          lines = lines.concat(res.body.split(/\n/).filter((l) => l.match(/^arangodb_collection_requests_bytes_(read|written)_total/)));
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
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to read something back
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);
          
          for (let i = 0; i < n; ++i) {
            c.document("test" + i);
          }
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertTrue(parsed.reads[shards[0]] > n * 40, {parsed, replicationFactor});
          assertTrue(parsed.reads[shards[0]] < n * 50, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenReadingFromCollectionSingleReadsLarge : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to read something back
          const n = 50;
          let payloadLength = 0;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            let payload = Array(n).join("abc");
            docs.push({ _key: "test" + i, payload });
            payloadLength += 10 + payload.length; // 10 for general overhead and attribute name
          }
          c.insert(docs);
          
          for (let i = 0; i < n; ++i) {
            c.document("test" + i);
          }
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertTrue(parsed.reads[shards[0]] > n * 40 + 0.95 * payloadLength, {parsed, replicationFactor});
          assertTrue(parsed.reads[shards[0]] < n * 50 + 1.05 * payloadLength, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + 0.95 * payloadLength * replicationFactor, {parsed, replicationFactor, payloadLength});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + 1.05 * payloadLength * replicationFactor, {parsed, replicationFactor, payloadLength});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenReadingFromCollectionBatchRead : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to read something back
          const n = 20;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);
         
          docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push("test" + i);
          }
          c.document(docs);
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertTrue(parsed.reads[shards[0]] > n * 40, {parsed, replicationFactor});
          assertTrue(parsed.reads[shards[0]] < n * 50, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleInserts : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          const n = 30;
          for (let i = 0; i < n; ++i) {
            c.insert({ value: i });
          }
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchInsert : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          const n = 25;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ value: i });
          }
          c.insert(docs);
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleRemoves : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to remove something later
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          for (let i = 0; i < n; ++i) {
            c.remove("test" + i);
          }
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // count 40-50 bytes for each insert, and 10-20 bytes for each remove
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 10 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 20 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchRemove : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to remove something later
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          c.remove(docs);
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // count 40-50 bytes for each insert, and 10-20 bytes for each remove
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 10 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 20 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleUpdates : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to updatee something later
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          for (let i = 0; i < n; ++i) {
            c.update("test" + i, { value: i + 1 });
          }
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // count 40-50 bytes for each insert, and 40-50 bytes for each update
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchUpdate : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to updatee something later
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i + 1 });
          }
          c.update(docs, docs);
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // count 40-50 bytes for each insert, and 40-50 bytes for each update
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchUpdateLarge : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to update something later
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          const payload = Array(1024).join("xyz");
          docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, payload });
          }
          c.update(docs, docs);
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // count 40-50 bytes for each insert, and 40-50 bytes for each update
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 3100 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 3150 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleReplaces : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to replace something later
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          for (let i = 0; i < n; ++i) {
            c.replace("test" + i, { value: i + 1 });
          }
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // count 40-50 bytes for each insert, and 40-50 bytes for each update
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionBatchReplace : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // must insert first to replace something later
          const n = 50;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i + 1 });
          }
          c.replace(docs, docs);
          
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // count 40-50 bytes for each insert, and 40-50 bytes for each update
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 40 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 50 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenWritingIntoCollectionSingleInsertsMultiShard : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {numberOfShards: 3, replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(3, shards.length);

          const n = 100;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value: i });
          }
          c.insert(docs);

          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);

          assertEqual(shards.length, Object.keys(parsed.writes).length, {replicationFactor, shards});
          let totalWritten = 0;
          Object.keys(parsed.writes).forEach((shard) => {
            totalWritten += parsed.writes[shard];
          });

          // count 40-50 bytes for each insert
          assertTrue(totalWritten > n * 40 * replicationFactor, {parsed, replicationFactor, shards, totalWritten});
          assertTrue(totalWritten < n * 50 * replicationFactor, {parsed, replicationFactor, shards, totalWritten});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsReadOnlyAQL : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        db._query(`FOR doc IN ${cn} RETURN doc`);
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertFalse(parsed.hasOwnProperty("reads"), parsed);
        assertFalse(parsed.hasOwnProperty("writes"), parsed);

        const n = 100;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        // run query again, now with documents
        db._query(`FOR doc IN ${cn} RETURN doc`);
          
        parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // we still assume 40-50 bytes read per document, as we still need to
        // read it entirely from the storage engine
        assertTrue(parsed.reads[shards[0]] > n * 40, {parsed});
        assertTrue(parsed.reads[shards[0]] < n * 50, {parsed});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQLLarge : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        const payload = Array(1024).join("xxx");
        const n = 100;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, payload });
        }
        c.insert(docs);
        
        db._query(`FOR doc IN ${cn} RETURN doc`);
          
        let parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // assume 3100-3150 bytes read per document 
        assertTrue(parsed.reads[shards[0]] > n * 3100, {parsed});
        assertTrue(parsed.reads[shards[0]] < n * 3150, {parsed});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQLProjection : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        const n = 100;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        db._query(`FOR doc IN ${cn} RETURN doc.value`);
          
        let parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // we still assume 40-50 bytes read per document, as we still need to
        // read it entirely from the storage engine
        assertTrue(parsed.reads[shards[0]] > n * 40, {parsed});
        assertTrue(parsed.reads[shards[0]] < n * 50, {parsed});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQLPrimaryIndexFullDocumentLookup : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        const n = 100;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        for (let i = 0; i < 10; ++i) {
          db._query(`FOR doc IN ${cn} FILTER doc._key == 'test${i}' RETURN doc.value`);
        }
          
        let parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // we still assume 40-50 bytes read per document, as we still need to
        // read it entirely from the storage engine
        assertTrue(parsed.reads[shards[0]] > 10 * 40, {parsed});
        assertTrue(parsed.reads[shards[0]] < 10 * 50, {parsed});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQLPrimaryIndexKeyOnlyLookup : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        const n = 100;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        for (let i = 0; i < 10; ++i) {
          db._query(`FOR doc IN ${cn} FILTER doc._key == 'test${i}' RETURN doc._key`);
        }
          
        let parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // we still assume 40-50 bytes read per document, as we still need to
        // read it entirely from the storage engine
        assertTrue(parsed.reads[shards[0]] > 10 * 40, {parsed});
        assertTrue(parsed.reads[shards[0]] < 10 * 50, {parsed});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsReadOnlyAQLLateMaterialize : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"] });

        let shards = c.shards();
        assertEqual(1, shards.length);

        const n = 500;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        db._query(`FOR doc IN ${cn} FILTER doc.value >= 25 RETURN doc`);
          
        let parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // assume 40-50 bytes read per document
        assertTrue(parsed.reads[shards[0]] > (n - 25) * 40, {parsed});
        assertTrue(parsed.reads[shards[0]] < (n - 25) * 50, {parsed});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsAQLSkipLimit : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        const n = 1000;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        db._query(`FOR doc IN ${cn} LIMIT ${n - 5}, 5 RETURN doc.value`);
          
        let parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // assume 40-50 bytes read per document
        assertTrue(parsed.reads[shards[0]] > 5 * 40, {parsed});
        assertTrue(parsed.reads[shards[0]] < 5 * 50, {parsed});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsAQLScanOnly : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn);
      try {
        let shards = c.shards();
        assertEqual(1, shards.length);

        const n = 1000;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        // do not actually read the documents
        db._query(`FOR doc IN ${cn} RETURN 1`);
          
        let parsed = getParsedMetrics(db._name(), cn);

        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        assertFalse(parsed.hasOwnProperty("reads"), parsed);
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

        db._query(`FOR doc IN ${cn} RETURN doc`);
        
        let parsed = getParsedMetrics(db._name(), cn);
        assertFalse(parsed.hasOwnProperty("reads"), parsed);
        assertFalse(parsed.hasOwnProperty("writes"), parsed);

        const n = 100;
        let docs = [];
        for (let i = 0; i < n; ++i) {
          docs.push({ _key: "test" + i, value: i });
        }
        c.insert(docs);
        
        // run query again, now with documents
        db._query(`FOR doc IN ${cn} RETURN doc`);
          
        parsed = getParsedMetrics(db._name(), cn);
          
        assertEqual(shards.length, Object.keys(parsed.writes).length, {shards});
        let totalRead = 0;
        Object.keys(parsed.reads).forEach((shard) => {
          totalRead += parsed.reads[shard];
        });

        // count 40-50 bytes for each document read
        assertTrue(totalRead > n * 40, {parsed, shards, totalRead});
        assertTrue(totalRead < n * 50, {parsed, shards, totalRead});
      } finally {
        db._drop(cn);
      }
    },
    
    testHasMetricsWriteAQL : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          const n = 100;
          db._query(`FOR i IN 1..${n} INSERT {} INTO ${cn}`);

          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);

          // count 30-40 bytes for each insert
          assertTrue(parsed.writes[shards[0]] > n * 30 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 40 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWriteAQLMultiShard : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {numberOfShards: 3, replicationFactor});

        try {
          let shards = c.shards();
          assertEqual(3, shards.length);

          const n = 100;
          db._query(`FOR i IN 1..${n} INSERT {} INTO ${cn}`);

          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);

          assertEqual(shards.length, Object.keys(parsed.writes).length, {replicationFactor, shards});
          let totalWritten = 0;
          Object.keys(parsed.writes).forEach((shard) => {
            totalWritten += parsed.writes[shard];
          });

          // count 30-40 bytes for each insert
          assertTrue(totalWritten > n * 30 * replicationFactor, {parsed, replicationFactor, shards, totalWritten});
          assertTrue(totalWritten < n * 40 * replicationFactor, {parsed, replicationFactor, shards, totalWritten});
        } finally {
          db._drop(cn);
        }
      });
    },

    testHasMetricsReadOnlyAQLMultiCollections : function () {
      let c1 = db._create(getUniqueCollectionName());
      let c2 = db._create(getUniqueCollectionName());
      try {
        const n = 100;
        db._query(`FOR i IN 1..${n} INSERT {} INTO ${c1.name()}`);

        // we must ensure that the execution order is doc1 -> doc2, and not vice versa.
        // otherwise we would get to different results
        db._query(`FOR doc1 IN ${c1.name()} FOR doc2 IN ${c2.name()} RETURN [doc1, doc2]`, null, {optimizer: {rules: ["-interchange-adjacent-enumerations"] } });
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        
        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // we assume 30-40 bytes read per document, as we still need to
        // read it entirely from the storage engine
        let shards = c1.shards();
        assertTrue(parsed.reads[shards[0]] > n * 30, {parsed});
        assertTrue(parsed.reads[shards[0]] < n * 40, {parsed});
       
        // we shouldn't have read any document from c2
        shards = c2.shards();
        assertFalse(parsed.reads.hasOwnProperty(shards[0]), {parsed, shards});
        
        // insert documents into c2
        db._query(`FOR i IN 1..${n / 2} INSERT {} INTO ${c2.name()}`);
        
        // run the read query again
        db._query(`FOR doc1 IN ${c1.name()} FOR doc2 IN ${c2.name()} RETURN [doc1, doc2]`, null, {optimizer: {rules: ["-interchange-adjacent-enumerations"] } });

        parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        
        assertTrue(parsed.hasOwnProperty("writes"), parsed);
        // we assume 30-40 bytes read per document, as we still need to
        // read it entirely from the storage engine
        shards = c1.shards();
        // c1 values should have doubled
        assertTrue(parsed.reads[shards[0]] > (n * 2) * 30, {parsed});
        assertTrue(parsed.reads[shards[0]] < (n * 2) * 40, {parsed});
        
        shards = c2.shards();
        // now we have reads for c2 as well
        assertTrue(parsed.reads[shards[0]] > (n * n / 2) * 30, {parsed});
        assertTrue(parsed.reads[shards[0]] < (n * n / 2) * 40, {parsed});
      } finally {
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsReadWriteAQLMultiCollections : function () {
      let c1 = db._create(getUniqueCollectionName(), {numberOfShards: 5, replicationFactor: 1});
      let c2 = db._create(getUniqueCollectionName(), {numberOfShards: 3, replicationFactor: 1});

      try {
        const n = 66;
        db._query(`FOR i IN 1..${n} INSERT {value: i} INTO ${c1.name()}`);

        const payload = Array(512).join("abcd");
        db._query(`LET payload = '${payload}' FOR doc IN ${c1.name()} INSERT {payload} INTO ${c2.name()} RETURN doc`);
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        
        let shards = c1.shards();
        let totalWritten = 0;
        let totalRead = 0;
        shards.forEach((shard) => {
          assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          assertTrue(parsed.reads.hasOwnProperty(shard), {shards, parsed});
          totalWritten += parsed.writes[shard];
          totalRead += parsed.reads[shard];
        });

        // count 40-50 bytes for each insert into c1
        assertTrue(totalWritten > n * 40, {parsed, shards, totalWritten});
        assertTrue(totalWritten < n * 50, {parsed, shards, totalWritten});
        
        assertTrue(totalRead > n * 40, {parsed, shards, totalRead});
        assertTrue(totalRead < n * 50, {parsed, shards, totalRead});
        
        shards = c2.shards();
        totalWritten = 0;
        shards.forEach((shard) => {
          assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          assertFalse(parsed.reads.hasOwnProperty(shard), {parsed, shards});
          totalWritten += parsed.writes[shard];
        });
        // count 2050-2150 bytes for each insert into c2
        assertTrue(totalWritten > n * 2050, {parsed, shards, totalWritten});
        assertTrue(totalWritten < n * 2150, {parsed, shards, totalWritten});
      } finally {
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsExclusiveAQLMultiCollections : function () {
      let c1 = db._create(getUniqueCollectionName(), {numberOfShards: 5, replicationFactor: 1});
      let c2 = db._create(getUniqueCollectionName(), {numberOfShards: 3, replicationFactor: 1});

      try {
        const n = 89;
        db._query(`FOR i IN 1..${n} INSERT {} INTO ${c1.name()} OPTIONS {exclusive: true} INSERT {} INTO ${c2.name()} OPTIONS {exclusive: true}`);
        
        let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
        assertFalse(parsed.hasOwnProperty("reads"), {parsed});
        
        let shards = c1.shards();
        shards.forEach((shard) => {
          assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
        });
        let totalWritten = 0;
        shards.forEach((shard) => {
          totalWritten += parsed.writes[shard];
        });

        // count 30-40 bytes for each insert into c1
        assertTrue(totalWritten > n * 30, {parsed, shards, totalWritten});
        assertTrue(totalWritten < n * 40, {parsed, shards, totalWritten});
        
        shards = c2.shards();
        totalWritten = 0;
        shards.forEach((shard) => {
          assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          totalWritten += parsed.writes[shard];
        });
        // count 30-40 bytes for each insert into c2
        assertTrue(totalWritten > n * 30, {parsed, shards, totalWritten});
        assertTrue(totalWritten < n * 40, {parsed, shards, totalWritten});
      } finally {
        db._drop(c2.name());
        db._drop(c1.name());
      }
    },
    
    testHasMetricsAQLFollowerShards : function () {
      [1, 2].forEach((replicationFactor) => {
        let c1 = db._create(getUniqueCollectionName(), {numberOfShards: 5, replicationFactor});
        let c2 = db._create(getUniqueCollectionName(), {numberOfShards: 3, replicationFactor});

        try {
          const n = 24;
          const payload = Array(100).join("foo");
          db._query(`LET payload = '${payload}' FOR i IN 1..${n} INSERT {} INTO ${c1.name()} INSERT {payload} INTO ${c2.name()}`);
        
          let parsed = getParsedMetrics(db._name(), [c1.name(), c2.name()]);
          assertFalse(parsed.hasOwnProperty("reads"), {parsed});
        
          let shards = c1.shards();
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          });
          let totalWritten = 0;
          shards.forEach((shard) => {
            totalWritten += parsed.writes[shard];
          });

          // count 30-40 bytes for each insert into c1
          assertTrue(totalWritten > n * 30 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
          assertTrue(totalWritten < n * 40 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
        
          shards = c2.shards();
          totalWritten = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
            totalWritten += parsed.writes[shard];
          });
          // count 360-370 bytes for each insert into c2
          assertTrue(totalWritten > n * 360 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
          assertTrue(totalWritten < n * 370 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
        } finally {
          db._drop(c2.name());
          db._drop(c1.name());
        }
      });
    },
    
    testHasMetricsSecondaryIndexes : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});
        try {
          c.ensureIndex({ type: "persistent", fields: ["value1"] });
          c.ensureIndex({ type: "persistent", fields: ["value2"] });
          let shards = c.shards();
          assertEqual(1, shards.length);

          const payload = Array(1024).join("z");
          const n = 42;
          let docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push({ _key: "test" + i, value1: "testmann" + i, value2: payload + i });
          }
          c.insert(docs);
          
          let parsed = getParsedMetrics(db._name(), cn);

          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          // we assume 1050-1150 bytes written per document
          assertTrue(parsed.writes[shards[0]] > n * 1050 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 1150 * replicationFactor, {parsed, replicationFactor});

          // read data back via secondary indexes. note that this returns _key so does full document lookups
          for (let i = 0; i < n; ++i) {
            db._query(`FOR doc IN ${cn} FILTER doc.value1 == 'testmann${i}' RETURN doc._key`);
          }
            
          parsed = getParsedMetrics(db._name(), cn);

          assertTrue(parsed.hasOwnProperty("writes"), parsed);
          assertTrue(parsed.reads[shards[0]] > n * 1050, {parsed});
          assertTrue(parsed.reads[shards[0]] < n * 1150, {parsed});
          
          // read data back via secondary indexes, but only return indexed value
          for (let i = 0; i < n; ++i) {
            db._query(`FOR doc IN ${cn} FILTER doc.value1 == 'testmann${i}' RETURN doc.value1`);
          }

          parsed = getParsedMetrics(db._name(), cn);

          // number of bytes read shouldn't have changed
          assertTrue(parsed.hasOwnProperty("writes"), parsed);
          assertTrue(parsed.reads[shards[0]] > n * 1050, {parsed});
          assertTrue(parsed.reads[shards[0]] < n * 1150, {parsed});
          
          // try to read back non-existing values
          for (let i = 0; i < n; ++i) {
            db._query(`FOR doc IN ${cn} FILTER doc.value2 == 'fuchsbau${i}' RETURN doc`);
          }

          parsed = getParsedMetrics(db._name(), cn);

          // number of bytes read shouldn't have changed
          assertTrue(parsed.hasOwnProperty("writes"), parsed);
          assertTrue(parsed.reads[shards[0]] > n * 1050, {parsed});
          assertTrue(parsed.reads[shards[0]] < n * 1150, {parsed});
          
          // remove all docs
          docs = [];
          for (let i = 0; i < n; ++i) {
            docs.push("test" + i);
          }

          c.remove(docs);
          
          parsed = getParsedMetrics(db._name(), cn);
          
          // we assume 1050-1150 bytes written per document (for the insert) plus a few bytes for each remove
          assertTrue(parsed.writes[shards[0]] > n * 1050 * replicationFactor + n * 10 * replicationFactor, {parsed, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 1150 * replicationFactor + n * 20 * replicationFactor, {parsed, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
   
    testHasMetricsMultipleDatabases : function () {
      const cn = getUniqueCollectionName();

      const databases = [baseName + "1", baseName + "2", baseName + "3"];

      try {
        let old = db._name();
        db._useDatabase('_system');
        databases.forEach((name) => {
          db._createDatabase(name);
        });
        db._useDatabase(old);

        databases.forEach((name, iter) => {
          db._useDatabase(name);
      
          let c = db._create(cn, {replicationFactor: 1});
          let shards = c.shards();
          assertEqual(1, shards.length);

          const n = (iter + 1) * 15;
          for (let i = 0; i < n; ++i) {
            c.insert({ value: i });
          }
        
          let parsed = getParsedMetrics(db._name(), cn);
          // count 40-50 bytes for each insert
          assertTrue(parsed.writes[shards[0]] > n * 40, {parsed, shards});
          assertTrue(parsed.writes[shards[0]] < n * 50, {parsed, shards});
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
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          // truncate an empty collection
          c.truncate();
        
          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), {parsed});
          assertFalse(parsed.hasOwnProperty("writes"), {parsed});
        
          // truncate a small collection
          const n = 100;
          db._query(`FOR i IN 1..${n} INSERT {value: i} INTO ${cn}`);
        
          parsed = getParsedMetrics(db._name(), cn);
          // count 40-50 bytes for each insert
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor, {parsed, shards, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor, {parsed, shards, replicationFactor});
        
          c.truncate();
         
          parsed = getParsedMetrics(db._name(), cn);
          // count 10-20 bytes for each remove
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor + n * 10 * replicationFactor, {parsed, shards, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor + n * 20 * replicationFactor, {parsed, shards, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenTruncatingLarge : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(1, shards.length);

          const n = 40000;
          db._query(`FOR i IN 1..${n} INSERT {value: i} INTO ${cn}`);
        
          let parsed = getParsedMetrics(db._name(), cn);
          // count 40-50 bytes for each insert
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor, {parsed, shards, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor, {parsed, shards, replicationFactor});
        
          c.truncate();
         
          parsed = getParsedMetrics(db._name(), cn);
          // truncate will have performed a DeleteRange - metrics should not have changed!
          assertTrue(parsed.writes[shards[0]] > n * 40 * replicationFactor, {parsed, shards, replicationFactor});
          assertTrue(parsed.writes[shards[0]] < n * 50 * replicationFactor, {parsed, shards, replicationFactor});
        } finally {
          db._drop(cn);
        }
      });
    },
   
    testHasMetricsWhenInsertIntoSmartGraph : function () {
      const isEnterprise = require("internal").isEnterprise();
      if (!isEnterprise) {
        return;
      }

      [1, 2].forEach((replicationFactor) => {
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

        graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor, smartGraphAttribute: "testi" });
        try {
          const n = 29;

          // smart vertex collection
          for (let i = 0; i < n; ++i) {
            db[vn].insert({ _key: "test" + (i % 10) + ":test" + i, testi: "test" + (i % 10) });
          }

          let parsed = getParsedMetrics(db._name(), vn);
          assertFalse(parsed.hasOwnProperty("reads"), {parsed});
        
          let shards = db[vn].shards();
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          });
          let totalWritten = 0;
          shards.forEach((shard) => {
            totalWritten += parsed.writes[shard];
          });

          // count 50-60 bytes for each insert into vn
          assertTrue(totalWritten > n * 50 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
          assertTrue(totalWritten < n * 60 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});

          // smart edge collection
          let keys = [];
          for (let i = 0; i < (n - 1); ++i) {
            keys.push(db[en].insert({ _from: vn + "/test" + i + ":test" + (i % 10), _to: vn + "/test" + ((i + 1) % 100) + ":test" + (i % 10), testi: (i % 10) })._key);
          }
          keys.push(db[en].insert({ _from: vn + "/test0:test0", _to: vn + "/testmann-does-not-exist:test0", testi: "test0" })._key);

          parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);

          // no insert into local part
          shards = db["_local_" + en].shards();
          shards.forEach((shard) => {
            assertFalse(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          });

          // we must have inserts into from/to parts
          shards = db["_from_" + en].shards();
          let totalFrom = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
            totalFrom += parsed.writes[shard];
          });
          
          shards = db["_to_" + en].shards();
          let totalTo = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
            totalTo += parsed.writes[shard];
          });

          // count 120-170 bytes for each insert into en
          assertTrue(totalFrom > n * 120 * replicationFactor, {parsed, shards, totalFrom, replicationFactor});
          assertTrue(totalFrom < n * 170 * replicationFactor, {parsed, shards, totalFrom, replicationFactor});
         
          assertTrue(totalTo > n * 120 * replicationFactor, {parsed, shards, totalTo, replicationFactor});
          assertTrue(totalTo < n * 170 * replicationFactor, {parsed, shards, totalTo, replicationFactor});
          
          // now perform reads
          keys.forEach((key) => {
            db[en].document(key);
          });

          parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);
          
          // no reads into local part
          shards = db["_local_" + en].shards();
          shards.forEach((shard) => {
            assertFalse(parsed.reads.hasOwnProperty(shard), {shards, parsed});
          });

          // we must have reads in from/to parts
          shards = db["_from_" + en].shards();
          totalFrom = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.reads.hasOwnProperty(shard), {shards, parsed});
            totalFrom += parsed.reads[shard];
          });
          
          shards = db["_to_" + en].shards();
          shards.forEach((shard) => {
            assertFalse(parsed.reads.hasOwnProperty(shard), {shards, parsed});
          });

          // count 120-170 bytes for each read
          assertTrue(totalFrom > n * 120, {parsed, shards, totalFrom});
          assertTrue(totalFrom < n * 170, {parsed, shards, totalFrom});
        } finally {
          cleanup();
        }
      });
    },
   
    testHasMetricsWhenAQLingSmartGraph : function () {
      const isEnterprise = require("internal").isEnterprise();
      if (!isEnterprise) {
        return;
      }
 
      [1, 2].forEach((replicationFactor) => {
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
        
        graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor, smartGraphAttribute: "testi" });
        try {
          const n = 24;
          db._query(`FOR i IN 1..${n} INSERT {_key: CONCAT('test', (i % 10), ':test', i), testi: CONCAT('test', (i % 10))} INTO ${vn}`);
          
          let parsed = getParsedMetrics(db._name(), vn);
          assertFalse(parsed.hasOwnProperty("reads"), {parsed});
        
          let shards = db[vn].shards();
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          });
          let totalWritten = 0;
          shards.forEach((shard) => {
            totalWritten += parsed.writes[shard];
          });

          // count 50-60 bytes for each insert into vn
          assertTrue(totalWritten > n * 50 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
          assertTrue(totalWritten < n * 60 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});

          let keys = db._query(`FOR i IN 1..${n} INSERT {_from: CONCAT('${vn}/test', i, ':test', (i % 10)), _to: CONCAT('${vn}/test', ((i + 1) % 100), ':test', (i % 10)), testi: (i % 10)} INTO ${en} RETURN NEW._key`).toArray();
          
          parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);
          
          // no insert into local part
          shards = db["_local_" + en].shards();
          shards.forEach((shard) => {
            assertFalse(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          });

          // we must have inserts into from/to parts
          shards = db["_from_" + en].shards();
          let totalFrom = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
            totalFrom += parsed.writes[shard];
          });
          
          shards = db["_to_" + en].shards();
          let totalTo = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
            totalTo += parsed.writes[shard];
          });

          db._query(`FOR doc IN ${en} FILTER doc._key IN @keys RETURN doc`, { keys });

          parsed = getParsedMetrics(db._name(), ["_from_" + en, "_to_" + en, "_local_" + en]);
         
          // no reads in local part
          shards = db["_local_" + en].shards();
          shards.forEach((shard) => {
            assertFalse(parsed.reads.hasOwnProperty(shard), {shards, parsed});
          });

          // we must have reads in from/to parts
          shards = db["_from_" + en].shards();
          totalFrom = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.reads.hasOwnProperty(shard), {shards, parsed});
            totalFrom += parsed.reads[shard];
          });
          
          shards = db["_to_" + en].shards();
          shards.forEach((shard) => {
            assertFalse(parsed.reads.hasOwnProperty(shard), {shards, parsed});
          });

          // count 120-170 bytes for each read
          assertTrue(totalFrom > n * 120, {parsed, shards, totalFrom});
          assertTrue(totalFrom < n * 170, {parsed, shards, totalFrom});
        } finally {
          cleanup();
        }

      });
    },
    
    testNoMetricsWhenUsingEmptyStreamingTrx : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {numberOfShards: 3, replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(3, shards.length);

          let trx = db._createTransaction({ collections: { write: cn } });

          try {
            let parsed = getParsedMetrics(db._name(), cn);
            assertFalse(parsed.hasOwnProperty("reads"), parsed);
            assertFalse(parsed.hasOwnProperty("writes"), parsed);
          } finally {
            trx.abort();
          }
        } finally {
          db._drop(cn);
        }
      });
    },
  
    testHasMetricsWhenUsingStreamingTrx : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {numberOfShards: 3, replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(3, shards.length);

          const n = 50;
          let trx = db._createTransaction({ collections: { write: cn } });

          try {
            let c = trx.collection(cn);

            for (let i = 0; i < n; ++i) {
              c.insert({ value: i });
            }
            
            let parsed = getParsedMetrics(db._name(), cn);
            assertFalse(parsed.hasOwnProperty("reads"), parsed);
          
            let totalWritten = 0;
            shards.forEach((shard) => {
              totalWritten += parsed.writes[shard];
            });
            assertTrue(totalWritten > n * 40 * replicationFactor, {parsed, replicationFactor, totalWritten});
            assertTrue(totalWritten < n * 50 * replicationFactor, {parsed, replicationFactor, totalWritten});

            // issue read query inside streaming trx
            trx.query(`FOR doc IN ${cn} RETURN doc`).toArray();
            
            parsed = getParsedMetrics(db._name(), cn);
          
            let totalRead = 0;
            totalWritten = 0;
            shards.forEach((shard) => {
              totalWritten += parsed.writes[shard];
              totalRead += parsed.reads[shard];
            });
            // total written should remain unchanged
            assertTrue(totalWritten > n * 40 * replicationFactor, {parsed, replicationFactor, totalWritten});
            assertTrue(totalWritten < n * 50 * replicationFactor, {parsed, replicationFactor, totalWritten});
            
            assertTrue(totalRead > n * 40, {parsed, totalRead});
            assertTrue(totalRead < n * 50, {parsed, totalRead});

            // write into the collection
            trx.query(`FOR i IN 1..5000 INSERT {} INTO ${cn}`);
            
            parsed = getParsedMetrics(db._name(), cn);
          
            totalRead = 0;
            totalWritten = 0;
            shards.forEach((shard) => {
              totalWritten += parsed.writes[shard];
              totalRead += parsed.reads[shard];
            });
            assertTrue(totalWritten > n * 40 * replicationFactor + 5000 * 30 * replicationFactor, {parsed, replicationFactor, totalWritten});
            assertTrue(totalWritten < n * 50 * replicationFactor + 5000 * 40 * replicationFactor, {parsed, replicationFactor, totalWritten});
            
            // total read should remain unchanged
            assertTrue(totalRead > n * 40, {parsed, totalRead});
            assertTrue(totalRead < n * 50, {parsed, totalRead});

          } finally {
            trx.abort();
          }
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsWhenUsingJavaScriptReadTrx : function () {
      const cn = getUniqueCollectionName();

      let c = db._create(cn, {numberOfShards: 3, replicationFactor: 1});
      try {
        let shards = c.shards();
        assertEqual(3, shards.length);

        const n = 50;

        db._query(`FOR i IN 0..${n - 1} INSERT {_key: CONCAT('test', i)} INTO ${cn}`);

        db._executeTransaction({ 
          collections: { write: cn }, 
          params: { cn, n }, 
          action: (params) => {
            let db = require("internal").db;
            let c = db._collection(params.cn);

            for (let i = 0; i < params.n; ++i) {
              c.document("test" + i);
            }
          }
        });

        let parsed = getParsedMetrics(db._name(), cn);
          
        let totalWritten = 0;
        let totalRead = 0;
        shards.forEach((shard) => {
          totalWritten += parsed.writes[shard];
          totalRead += parsed.reads[shard];
        });
        assertTrue(totalWritten > n * 20, {parsed, totalWritten});
        assertTrue(totalWritten < n * 40, {parsed, totalWritten});
        
        assertTrue(totalRead > n * 20, {parsed, totalWritten});
        assertTrue(totalRead < n * 40, {parsed, totalWritten});
      } finally {
        db._drop(cn);
      } 
    },
    
    testHasMetricsWhenUsingJavaScriptWriteTrx : function () {
      [1, 2].forEach((replicationFactor) => {
        const cn = getUniqueCollectionName();

        let c = db._create(cn, {numberOfShards: 3, replicationFactor});
        try {
          let shards = c.shards();
          assertEqual(3, shards.length);

          const n = 50;
          db._executeTransaction({ 
            collections: { write: cn }, 
            params: { cn, n }, 
            action: (params) => {
              let db = require("internal").db;
              let c = db._collection(params.cn);

              for (let i = 0; i < params.n; ++i) {
                c.insert({ value: i });
              }
            }
          });

          let parsed = getParsedMetrics(db._name(), cn);
          assertFalse(parsed.hasOwnProperty("reads"), parsed);
          
          let totalWritten = 0;
          shards.forEach((shard) => {
            totalWritten += parsed.writes[shard];
          });
          assertTrue(totalWritten > n * 40 * replicationFactor, {parsed, replicationFactor, totalWritten});
          assertTrue(totalWritten < n * 50 * replicationFactor, {parsed, replicationFactor, totalWritten});
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testHasMetricsGeneralGraphOperations : function () {
      [1, 2].forEach((replicationFactor) => {
        const vn = getUniqueCollectionName();
        const en = getUniqueCollectionName();
        const gn = "UnitTestsGraph";
        const graphs = require("@arangodb/general-graph");

        let cleanup = function () {
          try {
            graphs._drop(gn, true);
          } catch (err) {}
          db._drop(vn);
          db._drop(en);
        };
      
        graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 3, replicationFactor });
        try {
          const n = 29;

          let g = graphs._graph(gn);
          // vertex collection
          for (let i = 0; i < n; ++i) {
            g[vn].insert({ _key: "test" + i, value: i });
          }

          let parsed = getParsedMetrics(db._name(), vn);
          assertFalse(parsed.hasOwnProperty("reads"), {parsed});
        
          let shards = db[vn].shards();
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
          });
          let totalWritten = 0;
          shards.forEach((shard) => {
            totalWritten += parsed.writes[shard];
          });

          // count 40-50 bytes for each insert into vn
          assertTrue(totalWritten > n * 40 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
          assertTrue(totalWritten < n * 50 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});

          // edge collection
          for (let i = 0; i < (n - 1); ++i) {
            g[en].insert({ _key: "test" + i, _from: vn + "/test" + i, _to: vn + "/test" + i });
          }

          parsed = getParsedMetrics(db._name(), en);

          shards = db[en].shards();
          totalWritten = 0;
          shards.forEach((shard) => {
            assertTrue(parsed.writes.hasOwnProperty(shard), {shards, parsed});
            totalWritten += parsed.writes[shard];
          });

          // count 90-105 bytes for each insert into en
          assertTrue(totalWritten > n * 90 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
          assertTrue(totalWritten < n * 105 * replicationFactor, {parsed, shards, totalWritten, replicationFactor});
        } finally {
          cleanup();
        }
      });
    },

  };
}

function TestUser1Suite() {
  'use strict';

  const name = 'UnitTestsMetrics';
  const user = 'user1';

  const protocol = 'tcp';
  let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
  let oldUser = arango.connectedUser();

  let suite = {
    setUpAll: function () {
      users.save(user, "");
      users.grantDatabase(user, '_system', 'rw');

      db._createDatabase(name);
      db._useDatabase(name);

      users.grantDatabase(user, name, 'rw');
    
      arango.reconnect(endpoint, db._name(), user, '');
    },

    tearDownAll: function () {
      arango.reconnect(endpoint, '_system', oldUser, '');

      db._useDatabase("_system");
      db._dropDatabase(name);
      users.remove(user);
    },
  };

  deriveTestSuite(BaseTestSuite(user), suite, '_' + user);
  return suite;
}

function TestUser2Suite() {
  'use strict';

  const name = 'UnitTestsMetricsOther';
  const user = 'user2';

  const protocol = 'tcp';
  let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
  let oldUser = arango.connectedUser();

  let suite = {
    setUpAll: function () {
      users.save(user, "");
      users.grantDatabase(user, '_system', 'rw');

      db._createDatabase(name);
      db._useDatabase(name);

      users.grantDatabase(user, name, 'rw');
    
      arango.reconnect(endpoint, db._name(), user, '');
    },

    tearDownAll: function () {
      arango.reconnect(endpoint, '_system', oldUser, '');

      db._useDatabase("_system");
      db._dropDatabase(name);
      users.remove(user);
    },
  };

  deriveTestSuite(BaseTestSuite(user), suite, '_' + user);
  return suite;
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(TestUser1Suite);
  jsunity.run(TestUser2Suite);
}
return jsunity.done();
