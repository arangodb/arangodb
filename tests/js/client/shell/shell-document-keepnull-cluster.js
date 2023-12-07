/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual */

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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const request = require("@arangodb/request");
const db = require("@arangodb").db;
const {
  getCoordinators,
  getDBServers,
  waitForShardsInSync,
} = require('@arangodb/test-helper');

const cn = "UnitTestsCollection";

function UpdateKeepNullSuite() {
  return {
    setUp : function () {
      db._create(cn, { replicationFactor: 3 });
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testInsertOverwriteWithKeepNull : function () {
      db._query("INSERT { _key: 'test', a: 1, b: 2, c: 3 } INTO @@cn", { "@cn" : cn }); 
      db._query("INSERT { _key: 'test', a: null } IN @@cn OPTIONS { keepNull: false, overwriteMode: 'update' }", { "@cn" : cn }); 
      db._query("INSERT { _key: 'test', b: 3, d: 5 } IN @@cn OPTIONS { keepNull: false, overwriteMode: 'update' }", { "@cn" : cn }); 
      db._query("INSERT { _key: 'test', c: 4, d: null } IN @@cn OPTIONS { keepNull: false, overwriteMode: 'update' }", { "@cn" : cn }); 

      let doc = db[cn].document("test");
      let shards = db[cn].shards(true); 
      let shard = Object.keys(shards)[0]; 
      let servers = Object.values(shards)[0];

      getDBServers().forEach((server) => {
        if (!servers.includes(server.id)) {
          return;
        }
        let result = request({ method: "POST", url: server.url + "/_api/cursor", body: { query: "FOR doc IN @@cn RETURN doc", bindVars: { "@cn" : shard } }, json: true });
        assertEqual(201, result.status);
        
        result = result.json.result[0];
        assertFalse(result.hasOwnProperty("a"));
        assertEqual(3, result.b);
        assertEqual(4, result.c);
        assertFalse(result.hasOwnProperty("d"));
      });
          
      waitForShardsInSync(cn, 60, 1); 
    },

    testUpdateWithKeepNull : function () {
      db._query("INSERT { _key: 'test', a: 1, b: 2, c: 3 } INTO @@cn", { "@cn" : cn }); 
      db._query("UPDATE { _key: 'test' } WITH { a: null } IN @@cn OPTIONS { keepNull: false }", { "@cn" : cn }); 
      db._query("UPDATE { _key: 'test' } WITH { b: 3, d: 5 } IN @@cn OPTIONS { keepNull: false }", { "@cn" : cn }); 
      db._query("UPDATE { _key: 'test' } WITH { c: 4, d: null } IN @@cn OPTIONS { keepNull: false }", { "@cn" : cn }); 

      let doc = db[cn].document("test");
      let shards = db[cn].shards(true); 
      let shard = Object.keys(shards)[0]; 
      let servers = Object.values(shards)[0];

      getDBServers().forEach((server) => {
        if (!servers.includes(server.id)) {
          return;
        }
        let result = request({ method: "POST", url: server.url + "/_api/cursor", body: { query: "FOR doc IN @@cn RETURN doc", bindVars: { "@cn" : shard } }, json: true });
        assertEqual(201, result.status);
        
        result = result.json.result[0];
        assertFalse(result.hasOwnProperty("a"));
        assertEqual(3, result.b);
        assertEqual(4, result.c);
        assertFalse(result.hasOwnProperty("d"));
      });
          
      waitForShardsInSync(cn, 60, 1); 
    },
  };
}

jsunity.run(UpdateKeepNullSuite);

return jsunity.done();

