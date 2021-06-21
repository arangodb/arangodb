/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let fs = require('fs');
let pu = require('@arangodb/testutils/process-utils');
let db = arangodb.db;
let isCluster = require("internal").isCluster();

function restoreIntegrationSuite () {
  'use strict';
  const cn = 'UnitTestsRestore';
  // detect the path of arangorestore. quite hacky, but works
  const arangorestore = fs.join(global.ARANGOSH_PATH, 'arangorestore' + pu.executableExt);

  assertTrue(fs.isFile(arangorestore), "arangorestore not found!");

  let addConnectionArgs = function(args) {
    let endpoint = arango.getEndpoint().replace(/\+vpp/, '').replace(/^http:/, 'tcp:').replace(/^https:/, 'ssl:').replace(/^vst:/, 'tcp:').replace(/^h2:/, 'tcp:');
    args.push('--server.endpoint');
    args.push(endpoint);
    args.push('--server.database');
    args.push(arango.getDatabaseName());
    args.push('--server.username');
    args.push(arango.connectedUser());
  };

  let runRestore = function(path, args, rc) {
    args.push('--input-directory');
    args.push(path);
    addConnectionArgs(args);

    let actualRc = internal.executeExternalAndWait(arangorestore, args);
    assertTrue(actualRc.hasOwnProperty("exit"), actualRc);
    assertEqual(rc, actualRc.exit, actualRc);
  };

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testRestoreAutoIncrementKeyGenerator: function () {
      if (isCluster) {
        // auto-increment key-generator not supported on cluster
        return;
      }

      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2,
            keyOptions: { type: "autoincrement", lastValue: 12345, increment: 3, offset: 19 }
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let p = c.properties();
        assertEqual("autoincrement", p.keyOptions.type);
        assertEqual(12345, p.keyOptions.lastValue);
        assertEqual(3, p.keyOptions.increment);
        assertEqual(19, p.keyOptions.offset);

        let lastValue = p.keyOptions.lastValue;
        for (let i = 0; i < 10; ++i) {
          c.insert({});
          p = c.properties();
          let newLastValue = p.keyOptions.lastValue;
          assertTrue(newLastValue > lastValue);
          lastValue = newLastValue;
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestorePaddedKeyGenerator: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2,
            keyOptions: { type: "padded", lastValue: 12345 }
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let p = c.properties();
        assertEqual("padded", p.keyOptions.type);
        assertEqual(12345, p.keyOptions.lastValue);

        let lastValue = p.keyOptions.lastValue;
        for (let i = 0; i < 10; ++i) {
          c.insert({});
          p = c.properties();
          let newLastValue = p.keyOptions.lastValue;
          assertTrue(newLastValue > lastValue);
          lastValue = newLastValue;
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreWithRepeatedDocuments: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let data = [];
        for (let i = 0; i < 1000; ++i) {
          // will generate keys such as test0, test0, test1, test1 etc.
          data.push({ type: 2300, data: { _key: "test" + Math.floor(i / 2), value: i, overwrite: (i % 2 === 1) } });
        }

        fn = fs.join(path, cn + ".data.json");
        fs.write(fn, data.map((d) => JSON.stringify(d)).join('\n'));
        
        let args = ['--collection', cn, '--import-data', 'true'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        assertEqual(data.length / 2, c.count());
        for (let i = 0; i < data.length / 2; ++i) {
          let doc = c.document("test" + i);
          assertEqual((i * 2) + 1, doc.value);
          assertTrue(doc.overwrite);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreInsertRemove: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let data = [];
        for (let i = 0; i < 10; ++i) {
          data.push({ type: 2300, data: { _key: "test" + i, value: i, old: true } });
        }
        for (let i = 0; i < 6; ++i) {
          data.push({ type: 2302, key: "test" + i });
        }
        for (let i = 4; i < 7; ++i) {
          data.push({ type: 2300, data: { _key: "test" + i, value: i * 2, overwrite: true } });
        }

        fn = fs.join(path, cn + ".data.json");
        fs.write(fn, data.map((d) => JSON.stringify(d)).join('\n'));
        
        let args = ['--collection', cn, '--import-data', 'true', '--overwrite', 'true'];
        runRestore(path, args, 0); 

        let c = db._collection(cn), count = c.count();
        assertEqual(6, count);
        for (let i = 0; i < 4; ++i) {
          assertFalse(c.exists("test" + i));
        }
        for (let i = 4; i < 7; ++i) {
          let doc = c.document("test" + i);
          assertEqual(i * 2, doc.value);
          assertTrue(doc.overwrite);
          assertFalse(doc.hasOwnProperty('old'));
        }
        for (let i = 8; i < 10; ++i) {
          let doc = c.document("test" + i);
          assertEqual(i, doc.value);
          assertTrue(doc.old);
          assertFalse(doc.hasOwnProperty('overwrite'));
        }  
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreWithLineBreaksInData: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let data = [];
        for (let i = 0; i < 1000; ++i) {
          data.push({ type: 2300, data: { _key: "test" + i, value: i } });
        }

        fn = fs.join(path, cn + ".data.json");
        fs.write(fn, data.map((d) => '\n' + JSON.stringify(d)).join('\n\n'));
        
        let args = ['--collection', cn, '--import-data', 'true'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        assertEqual(data.length, c.count());
        for (let i = 0; i < data.length; ++i) {
          let doc = c.document("test" + i);
          assertEqual(i, doc.value);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreWithEnvelopesWithDumpJsonFile: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let data = [];
        for (let i = 0; i < 5000; ++i) {
          data.push({ type: 2300, data: { _key: "test" + i, value: i } });
        }

        fn = fs.join(path, cn + ".data.json");
        fs.write(fn, data.map((d) => JSON.stringify(d)).join('\n'));
        
        fn = fs.join(path, "dump.json");
        fs.write(fn, JSON.stringify({
          useEnvelopes: true
        }));
        
        let args = ['--collection', cn, '--import-data', 'true'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        assertEqual(data.length, c.count());
        for (let i = 0; i < data.length; ++i) {
          let doc = c.document("test" + i);
          assertEqual(i, doc.value);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreWithEnvelopesNoDumpJsonFile: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let data = [];
        for (let i = 0; i < 5000; ++i) {
          data.push({ type: 2300, data: { _key: "test" + i, value: i } });
        }

        fn = fs.join(path, cn + ".data.json");
        fs.write(fn, data.map((d) => JSON.stringify(d)).join('\n'));
        
        let args = ['--collection', cn, '--import-data', 'true'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        assertEqual(data.length, c.count());
        for (let i = 0; i < data.length; ++i) {
          let doc = c.document("test" + i);
          assertEqual(i, doc.value);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreWithoutEnvelopesWithDumpJsonFile: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let data = [];
        for (let i = 0; i < 5000; ++i) {
          data.push({ _key: "test" + i, value: i });
        }

        fn = fs.join(path, cn + ".data.json");
        fs.write(fn, data.map((d) => JSON.stringify(d)).join('\n'));
        
        fn = fs.join(path, "dump.json");
        fs.write(fn, JSON.stringify({
          useEnvelopes: false
        }));
        
        let args = ['--collection', cn, '--import-data', 'true'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        assertEqual(data.length, c.count());
        for (let i = 0; i < data.length; ++i) {
          let doc = c.document("test" + i);
          assertEqual(i, doc.value);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreWithoutEnvelopesNoDumpJsonFile: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let data = [];
        for (let i = 0; i < 5000; ++i) {
          data.push({ _key: "test" + i, value: i });
        }

        fn = fs.join(path, cn + ".data.json");
        fs.write(fn, data.map((d) => JSON.stringify(d)).join('\n'));
        
        let args = ['--collection', cn, '--import-data', 'true'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        assertEqual(data.length, c.count());
        for (let i = 0; i < data.length; ++i) {
          let doc = c.document("test" + i);
          assertEqual(i, doc.value);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreNumericGloballyUniqueId: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            globallyUniqueId: "123456789012",
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        assertNotEqual("123456789012", c.properties().globallyUniqueId);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreIndexesOldFormat: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            indexes: [
              { id: "0", fields: ["_key"], type: "primary", unique: true },
              { id: "95", fields: ["loc"], type: "geo", geoJson: false },
              { id: "295", fields: ["value"], type: "skiplist", sparse: true },
            ],
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(3, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);
        assertEqual("geo", indexes[1].type);
        assertEqual(["loc"], indexes[1].fields);
        assertFalse(indexes[1].geoJson);
        assertEqual("skiplist", indexes[2].type);
        assertEqual(["value"], indexes[2].fields);

        // test if the indexes work
        for (let i = 0; i < 100; ++i) {
          c.insert({ _key: "test" + i, value: 42 });
        }
        for (let i = 0; i < 100; ++i) {
          assertEqual("test" + i, c.document("test" + i)._key);
        }
        let result = db._query("FOR doc IN " + cn + " FILTER doc.value == 42 RETURN doc").toArray();
        assertEqual(100, result.length);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreIndexesOldFormatGeo1: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            indexes: [
              { id: "95", fields: ["loc"], type: "geo1", geoJson: false },
            ],
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);
        assertEqual("geo", indexes[1].type);
        assertEqual(["loc"], indexes[1].fields);
        assertFalse(indexes[1].geoJson);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreIndexesOldFormatGeo2: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            indexes: [
              { id: "95", fields: ["a", "b"], type: "geo2", geoJson: false },
            ],
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);
        assertEqual("geo", indexes[1].type);
        assertEqual(["a", "b"], indexes[1].fields);
        assertFalse(indexes[1].geoJson);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },

    testRestoreIndexesFulltextLengthZero: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            indexes: [
              { id: "95", fields: ["text"], type: "fulltext", minLength: 0 },
            ],
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);
        assertEqual("fulltext", indexes[1].type);
        assertEqual(["text"], indexes[1].fields);
        assertEqual(indexes[1].minLength, 1);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreIndexesNewFormat: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [
            { id: "95", fields: ["loc"], type: "geo", geoJson: false },
            { id: "295", fields: ["value"], type: "skiplist", sparse: true },
          ],
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 2
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(3, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);
        assertEqual("geo", indexes[1].type);
        assertEqual(["loc"], indexes[1].fields);
        assertFalse(indexes[1].geoJson);
        assertEqual("skiplist", indexes[2].type);
        assertEqual(["value"], indexes[2].fields);

        // test if the indexes work
        for (let i = 0; i < 100; ++i) {
          c.insert({ _key: "test" + i, value: 42 });
        }
        for (let i = 0; i < 100; ++i) {
          assertEqual("test" + i, c.document("test" + i)._key);
        }
        let result = db._query("FOR doc IN " + cn + " FILTER doc.value == 42 RETURN doc").toArray();
        assertEqual(100, result.length);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },

    testRestoreEdgeIndexOldFormat: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            indexes: [
              { id: "0", fields: ["_key"], type: "primary", unique: true },
              { id: "1", fields: ["_from", "_to"], type: "edge" },
              { id: "95", fields: ["value"], type: "hash" },
            ],
            name: cn,
            numberOfShards: 3,
            type: 3 // edge collection
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(3, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);
        assertEqual("edge", indexes[1].type);
        assertEqual(["_from", "_to"], indexes[1].fields);
        assertEqual("hash", indexes[2].type);
        assertEqual(["value"], indexes[2].fields);

        // test if the indexes work
        for (let i = 0; i < 100; ++i) {
          c.insert({ _key: "test" + i, _from: "v/" + i, _to: "v/" + i, value: 42 });
        }
        for (let i = 0; i < 100; ++i) {
          assertEqual("test" + i, c.document("test" + i)._key);
        }
        for (let i = 0; i < 100; ++i) {
          let inEdges = c.inEdges("v/" + i);
          assertEqual(1, inEdges.length);
          assertEqual("test" + i, inEdges[0]._key);
          
          let outEdges = c.outEdges("v/" + i);
          assertEqual(1, outEdges.length);
          assertEqual("test" + i, outEdges[0]._key);
        }
        let result = db._query("FOR doc IN " + cn + " FILTER doc.value == 42 RETURN doc").toArray();
        assertEqual(100, result.length);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreEdgeIndexWrongCollectionType: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [],
          parameters: {
            indexes: [
              { id: "0", fields: ["_key"], type: "primary", unique: true },
              { id: "1", fields: ["_from", "_to"], type: "edge" },
            ],
            name: cn,
            numberOfShards: 3,
            type: 2 // document collection
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(1, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);

        // test if the index works
        for (let i = 0; i < 100; ++i) {
          c.insert({ _key: "test" + i });
        }
        for (let i = 0; i < 100; ++i) {
          assertEqual("test" + i, c.document("test" + i)._key);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testRestoreEdgeIndexNewFormat: function () {
      let path = fs.getTempFile();
      try {
        fs.makeDirectory(path);
        let fn = fs.join(path, cn + ".structure.json");

        fs.write(fn, JSON.stringify({
          indexes: [], // no edge index here, as it is an automatic index!
          parameters: {
            name: cn,
            numberOfShards: 3,
            type: 3 // edge collection
          }
        }));

        let args = ['--collection', cn, '--import-data', 'false'];
        runRestore(path, args, 0); 

        let c = db._collection(cn);
        let indexes = c.indexes();
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        assertEqual(["_key"], indexes[0].fields);
        assertEqual("edge", indexes[1].type);
        assertEqual(["_from", "_to"], indexes[1].fields);

        // test if the indexes work
        for (let i = 0; i < 100; ++i) {
          c.insert({ _key: "test" + i, _from: "v/" + i, _to: "v/" + i, value: 42 });
        }
        for (let i = 0; i < 100; ++i) {
          assertEqual("test" + i, c.document("test" + i)._key);
        }
        for (let i = 0; i < 100; ++i) {
          let inEdges = c.inEdges("v/" + i);
          assertEqual(1, inEdges.length);
          assertEqual("test" + i, inEdges[0]._key);
          
          let outEdges = c.outEdges("v/" + i);
          assertEqual(1, outEdges.length);
          assertEqual("test" + i, outEdges[0]._key);
        }
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
  };
}

jsunity.run(restoreIntegrationSuite);

return jsunity.done();
