/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global arango, assertEqual, assertTrue*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test the replication interface
///
/// @file
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
/// @author Jan Christoph Uhde
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;

function ReplicationApiSuite () {
  'use strict';

  var cn = "UnitTestsCollection";
  var collection = null;
  var batchesToFree = [];

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn);
      let docs = [];
      for(var i = 0; i < 100; i++){
        docs.push({"number":i});
      }
      collection.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // avoid hanging tests! by canceling batches
      batchesToFree.forEach( function(id){
          arango.DELETE("/_api/replication/batch/"+ id);
      });

      collection.drop();
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testCreateBatchId : function () {
      let result = arango.POST("/_api/replication/batch", {});
      assertTrue(result.hasOwnProperty("id"));

      // delete batch
      result = arango.DELETE("/_api/replication/batch/"+ result.id);
      assertEqual(204, result.code);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testKeys : function () {
      // create batch
      let result = arango.POST("/_api/replication/batch", {});
      assertTrue(result.hasOwnProperty("id"));
      batchesToFree.push(result.id);
      let batchId = result.id;

      // create keys
      result = arango.POST("/_api/replication/keys?collection=" + cn + 
                           "&batchId=" + result.id, {});
      assertTrue(result.hasOwnProperty("count"));
      assertTrue(result.hasOwnProperty("id"));
      assertTrue(result.count === 100);
      let id = result.id;

      // fetch keys
      result = arango.PUT("/_api/replication/keys/"+ id +
                          "?collection=" + cn + "&type=keys", {});

      assertTrue(Array.isArray(result));

      result = arango.PUT("/_api/replication/keys/"+ id +
                          "?collection=" + cn +
                          "&type=keys" +
                          "&chunk=" + Math.pow(2,60) +
                              "&chunkSize=" + Math.pow(2,60), {});
      assertEqual(400, result.code);

      // iterator should be invalid
      result = arango.PUT("/_api/replication/keys/"+ id +
                          "?collection=" + cn +
                          "&type=keys" +
                          "&chunk=" + 5 +
                          "&chunkSize=" + 2000, {});
      assertEqual(400, result.code);

      // delete batch
      result = arango.DELETE("/_api/replication/batch/" + batchId);
      assertEqual(204, result.code);

      batchesToFree.pop();
    }

  }; // return
} // Replication suite

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ReplicationApiSuite);

return jsunity.done();
