/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, arango */

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
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = internal.db;
const versionHas = require("@arangodb/test-helper").versionHas;
let IM = global.instanceManager;

function IndexInBackgroundFailuresSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  let collection = null;
  
  let run = function(insertData, getRanges) {
    const start = internal.time();

    let maxArchivedLogNumber = null;
    let ranges = getRanges();
    if (ranges.length) {
      maxArchivedLogNumber = ranges[ranges.length - 1];
    }

    let timeout = 600;
    if (versionHas('asan') || versionHas('tsan') || versionHas('coverage')) {
      timeout *= 10;
    }

    let newFiles = 0;
    while (true) {
      insertData();
    
      let ranges = getRanges();
      if (ranges.length) {
        let max = ranges[ranges.length - 1];
        if (maxArchivedLogNumber === null || max > maxArchivedLogNumber) {
          maxArchivedLogNumber = max;
          ++newFiles;
          require("console").warn("new WAL files:", newFiles, ranges);
        }
      }

      if (newFiles >= 8) {
        // if we have at least 8 new WAL files, we have enough
        break;
      }

      assertFalse(internal.time() - start > timeout, "time's up for this test!");
    }
  };

  return {

    setUp : function () {
      IM.debugClearFailAt();
      db._drop(cn);
      collection = db._create(cn);
    },

    tearDown : function () {
      IM.debugClearFailAt();
      db._drop(cn);
    },

    // this test sets a failure point to delay background index creation.
    // background index creation is started via an async call, but the thread 
    // that performs the background index creation will be blocked, waiting
    // for the failure point to be removed.
    // the test code will then insert documents into the underlying collection
    // and fill up 8 WAL files with them. the failure point will also lead
    // to obsolete WAL files being pruned aggressively.
    // after 8 WAL files have been created, the failure point will be removed
    // and the actual index creation can happen using the remaining WAL files.
    // it is expected that the index data will always be complete, i.e.
    // contain an index entry for every document in the collection, even if
    // WAL file pruning ran during index creation.
    // this tests that the background indexing keeps a hold on the WAL files
    // that contain new documents that have been added to the collection after
    // background index creation has started.
    testPurgeWalWhileBackgroundIndexing : function () {
      // arbitrary documents to fill up WAL files
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: "testmann" + i, value2: i, value3: internal.genRandomAlphaNumbers(100) });
      }

      internal.wal.flush(true, true);

      IM.debugSetFailAt("BuilderIndex::purgeWal");

      // create an index in background. note that due to the failure point,
      // index creation will block until we remove the failure point
      let res = arango.POST_RAW(`/_api/index?collection=${encodeURIComponent(cn)}`, 
        { type: "persistent", fields: ["value1"], inBackground: true }, { "x-arango-async" : "store" });
      
      assertTrue(res.headers.hasOwnProperty("x-arango-async-id"));
      const id = res.headers["x-arango-async-id"];
      
      let getRanges = function() {
        return require("@arangodb/replication").logger.tickRanges().filter(function(r) {
          return r.status === 'collected';
        }).map(function(r) {
          return parseInt(r.datafile.replace(/^.*?(\d+)\.log$/, "$1"));
        });
      };
      
      let insertData = function() {
        collection.insert(docs);
      };

      // insert new documents into the WAL that the index creation could miss
      run(insertData, getRanges);

      // resume index creation
      IM.debugClearFailAt();

      // wait until index is there...
      let tries = 0;
      while (++tries < 300) {
        res = arango.PUT_RAW("/_api/job/" + id, "");
        if (res.code === 201 || res.code >= 400) {
          break;
        }
        internal.sleep(0.5);
      }

      assertEqual(201, res.code);
   
      // verify that the number of index entries equals the number of documents
      let figures = collection.figures(true);
      assertEqual(2, figures.engine.indexes.length);
      assertEqual(figures.engine.documents, figures.engine.indexes[1].count, figures);
    },

  };
}

jsunity.run(IndexInBackgroundFailuresSuite);
return jsunity.done();
