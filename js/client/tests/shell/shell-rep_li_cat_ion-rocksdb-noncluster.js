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
      for(var i = 0; i < 100; i++){
        collection.insert({"number":i});
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // avoid hanging tests! by canceling batches
      batchesToFree.forEach( function(id){
          arango.DELETE_RAW("/_api/replication/batch/"+ id, "");
      });

      collection.drop();
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testCreateBatchId : function () {
      // create batch
      var doc = {};
      var result = arango.POST_RAW("/_api/replication/batch", JSON.stringify(doc));
      assertEqual(200, result.code);
      var obj = JSON.parse(result.body);
      assertTrue(obj.hasOwnProperty("id"));

      // delete batch
      result = arango.DELETE_RAW("/_api/replication/batch/"+ obj.id, "");
      assertEqual(204, result.code);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testKeys : function () {
      // create batch
      var doc = {};
      var result = arango.POST_RAW("/_api/replication/batch", JSON.stringify(doc));
      assertEqual(200, result.code);
      var batchObj = JSON.parse(result.body);
      assertTrue(batchObj.hasOwnProperty("id"));
      batchesToFree.push(batchObj.id);

      // create keys
      result = arango.POST_RAW("/_api/replication/keys?collection=" + cn + 
                                   "&batchId=" + batchObj.id, JSON.stringify(doc));
      assertEqual(200, result.code);
      var keysObj = JSON.parse(result.body);
      assertTrue(keysObj.hasOwnProperty("count"));
      assertTrue(keysObj.hasOwnProperty("id"));
      assertTrue(keysObj.count === 100);

      // fetch keys
      result = arango.PUT_RAW("/_api/replication/keys/"+ batchObj.id +
                              "?collection=" + cn +
                              "&type=keys"
                             ,JSON.stringify(doc)
                             );

      assertEqual(200, result.code);
      keysObj = JSON.parse(result.body);
      assertTrue(Array.isArray(keysObj));

      result = arango.PUT_RAW("/_api/replication/keys/"+ batchObj.id +
                              "?collection=" + cn +
                              "&type=keys" +
                              "&chunk=" + Math.pow(2,60) +
                              "&chunkSize=" + Math.pow(2,60)
                             ,JSON.stringify(doc)
                             );
      assertEqual(400, result.code);

      // iterator should be invalid
      result = arango.PUT_RAW("/_api/replication/keys/"+ batchObj.id +
                              "?collection=" + cn +
                              "&type=keys" +
                              "&chunk=" + 5 +
                              "&chunkSize=" + 2000
                             ,JSON.stringify(doc)
                             );
      assertEqual(400, result.code);

      // delete batch
      result = arango.DELETE_RAW("/_api/replication/batch/"+ batchObj.id, "");
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
