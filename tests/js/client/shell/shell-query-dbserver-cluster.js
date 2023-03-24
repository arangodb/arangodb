/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const db = require("internal").db;
const url = require('url');
const _ = require("lodash");
const { getDBServers } = require('@arangodb/test-helper');

const cn = "UnitTestsQueries";
const originalEndpoint = arango.getEndpoint();

function queriesTestSuite () {
  'use strict';
  
  return {
    setUpAll: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
      db._drop(cn);
      
      let c = db._create(cn, { numberOfShards: 5 });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value : i });
      }
      c.insert(docs);
    },

    tearDownAll: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
      db._drop(cn);
    },

    setUp: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
    },

    tearDown: function() {
      arango.reconnect(originalEndpoint, "_system", arango.connectedUser(), "");
    },
    
    // test executing operations on the coordinator
    testCoordinator: function() {
      assertEqual(100, db[cn].count());
      assertEqual(100, db[cn].toArray().length);
      assertEqual(100, db._query("FOR doc IN " + cn + " RETURN doc").toArray().length);
    },

    // test executing operations on the DB-Server, on all individual shards
    testDBServer: function() {
      let shardMap = db[cn].shards(true);
      assertEqual(5, Object.keys(shardMap).length, shardMap);
      
      const dbservers = getDBServers();
      assertTrue(dbservers.length > 0, "no dbservers found");
        
      let totalCount = 0, totalToArray = 0, totalQuery = 0;
       
      dbservers.forEach(function(dbserver, i) {
        let id = dbserver.id;
        require("console").warn("connecting to dbserver", dbserver.endpoint, id);
        arango.reconnect(dbserver.endpoint, "_system", arango.connectedUser(), "");

        let shards = Object.keys(shardMap).filter(function(shard) {
          return shardMap[shard][0] === id;
        });
        shards.forEach(function(shard) {
          totalCount += db[shard].count();
          totalToArray += db[shard].toArray().length;
          totalQuery += db._query("FOR doc IN " + shard + " RETURN doc").toArray().length;
        });
      });

      assertEqual(100, totalCount);
      assertEqual(100, totalToArray);
      assertEqual(100, totalQuery);
    },
    
    // test executing operations on the coordinator, without collection
    testCoordinatorNoCollection: function() {
      let result = db._query("RETURN [DECODE_REV('_dpq8a-----'), DECODE_REV('_bpq8a-----')]").toArray();

      assertEqual([
        [ 
          { "date" : "2022-02-02T16:22:18.368Z", "count" : 0 }, 
          { "date" : "2021-01-01T00:00:00.000Z", "count" : 0 } 
        ]
      ], result);
    },
    
    // test executing operations on the DB-Server, without collection
    testDBServerNoCollection: function() {
      const dbservers = getDBServers();
      assertTrue(dbservers.length > 0, "no dbservers found");
        
      dbservers.forEach(function(dbserver, i) {
        let id = dbserver.id;
        require("console").warn("connecting to dbserver", dbserver.endpoint, id);
        arango.reconnect(dbserver.endpoint, "_system", arango.connectedUser(), "");

        let result = db._query("RETURN [DECODE_REV('_dpq8a-----'), DECODE_REV('_bpq8a-----')]").toArray();

        assertEqual([
          [ 
            { "date" : "2022-02-02T16:22:18.368Z", "count" : 0 }, 
            { "date" : "2021-01-01T00:00:00.000Z", "count" : 0 } 
          ]
        ], result);
      });
    },

  };
}

jsunity.run(queriesTestSuite);
return jsunity.done();
