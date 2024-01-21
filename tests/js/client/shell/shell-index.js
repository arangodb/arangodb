/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief test index methods
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const arango = internal.arango;
const errors = internal.errors;
const { helper, versionHas } = require("@arangodb/test-helper");
const platform = require('internal').platform;


const cn = "UnitTestsCollection";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: basics
////////////////////////////////////////////////////////////////////////////////

function IndexSuite() {
  'use strict';

  let collection = null;

  return {

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown : function () {
      internal.db._drop(cn);
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: indexes
////////////////////////////////////////////////////////////////////////////////

    testIndexProgress : function () {
      var c = internal.db._create("c0l", {numberOfShards: 3});
      var docs = [];
      const sleep = require('internal').sleep;
      for (var j = 0; j < 50; ++j) {
        docs=[];
        for (var i = 0; i < 10240; ++i) {
          docs.push({name : "name" + i});
        }
        c.insert(docs);
      }
      
      internal.debugSetFailAt("fillIndex::pause");
      var idx = { name:"progress", type: "persistent", fields:["name"], inBackground: true};
      arango.GET(`/_api/index?collection=c0l`);

      var job = arango.POST_RAW(`/_api/index?collection=c0l`, idx,
                                {"x-arango-async": "store"}).headers["x-arango-async-id"];
      let count = 0;
      var progress = 0.0;
      var idxs = [];
      while (true) {
        idxs = arango.GET(`/_api/index?collection=c0l&withHidden=true`).indexes;
        if (idxs.length > 1) {
          break;
        }
        sleep(0.05);
      }
      while (arango.GET(`/_api/index?collection=c0l&withHidden=true`) &&
             arango.GET(`/_api/index?collection=c0l&withHidden=true`).indexes[1].hasOwnProperty("progress")) {
        while (true) {
          idxs = arango.GET(`/_api/index?collection=c0l&withHidden=true`).indexes;
          if (idxs[1].hasOwnProperty("progress") && idxs[1].progress > progress) {
            progress = idxs[1].progress;
            console.warn(Math.round(progress) + "%");
            internal.debugSetFailAt("fillIndex::unpause");
            internal.debugSetFailAt("fillIndex::next");
            break;
          }
          sleep (0.1);
        }
        sleep(0.1);
        internal.debugRemoveFailAt("fillIndex::unpause");
//        sleep(0.2);
//        internal.debugSetFailAt("fillIndex::next");
        sleep(0.1);
        internal.debugRemoveFailAt("fillIndex::next");
      }

      // Clear fail points
      internal.debugRemoveFailAt("fillIndex::pause");
      internal.debugSetFailAt("fillIndex::unpause");
      internal.debugRemoveFailAt("fillIndex::unpause");
      internal.debugSetFailAt("fillIndex::next");
      internal.debugRemoveFailAt("fillIndex::next");

      // Clean up
      c.drop();
    },
  };
}

jsunity.run(IndexSuite);

return jsunity.done();
