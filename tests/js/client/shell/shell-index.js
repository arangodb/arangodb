/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief test index methods
///
/// DISCLAIMER
///
/// Copyright 2024 ArangoDB GmbH
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const arango = internal.arango;
const errors = internal.errors;
const { helper, versionHas } = require("@arangodb/test-helper");
const platform = require('internal').platform;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: basics
////////////////////////////////////////////////////////////////////////////////

function IndexSuite() {
  'use strict';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test: indexes
////////////////////////////////////////////////////////////////////////////////

    testIndexProgress : function () {
      let c = internal.db._create("c0l", {numberOfShards: 3, replicationFactor: 2});
      let docs = [];
      const sleep = require('internal').sleep;
      for (let j = 0; j < 16; ++j) {
        docs=[];
        for (let i = 0; i < 10240; ++i) {
          docs.push({name : "name" + i});
        }
        c.insert(docs);
      }
      
      try {
        internal.debugSetFailAt("fillIndex::pause");
        let idx = { name:"progress", type: "persistent", fields:["name"], inBackground: true};
        arango.GET(`/_api/index?collection=c0l`);

        let job = arango.POST_RAW(`/_api/index?collection=c0l`, idx,
                                  {"x-arango-async": "store"}).headers["x-arango-async-id"];
        let count = 0;
        let progress = 0.0;
        let idxs = [];
        // Wait until the index is there (with withHidden):
        while (true) {
          idxs = arango.GET(`/_api/index?collection=c0l&withHidden=true`).indexes;
          if (idxs.length > 1) {
            break;
          }
          sleep(0.1);
        }

        let unpaused = false;
        while (true) {
          idx = arango.GET(`/_api/index?collection=c0l&withHidden=true`).indexes[1];
          if (idx.hasOwnProperty("progress")) {
            assertTrue(idx.progress >= progress);
            assertTrue(idx.isBuilding);
            progress = idx.progress;
            if (progress > 0 && !unpaused) {
              // Only release index building once we have seen at least
              // once an isBuilding state with non-zero progress
              internal.debugSetFailAt("fillIndex::unpause");
              unpaused = true;
            }
            sleep(0.1);
          } else {
            assertFalse(idx.hasOwnProperty("isBuilding"));
            break;
          }
        }
      } finally {
        internal.debugRemoveFailAt("fillIndex::pause");
        internal.debugRemoveFailAt("fillIndex::unpause");
        c.drop();
      }
    },
  };
}

jsunity.run(IndexSuite);

return jsunity.done();
