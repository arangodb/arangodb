/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");
const request = require('@arangodb/request');
const ERRORS = require('internal').errors;

function AuthSuite() {
  'use strict';
  let baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  const user1 = 'hackers@arangodb.com';
  const user2 = 'noone@arangodb.com';
  let servers = [];
      
  let checkClusterHealth = function() {
    let result = arango.GET('/_admin/cluster/health');
    assertTrue(result.hasOwnProperty('ClusterId'), result);
    assertTrue(result.hasOwnProperty('Health'), result);
  };

  let checkClusterNodeVersion = function() {
    // this will just redirect to a new endpoint
    let result = arango.GET('/_admin/clusterNodeVersion?ServerID=' + servers[0]);
    assertEqual(308, result.code);
    
    result = arango.GET('/_admin/cluster/nodeVersion?ServerID=' + servers[0]);
    assertTrue(result.hasOwnProperty('server'), result);
    assertTrue(result.hasOwnProperty('license'), result);
    assertTrue(result.hasOwnProperty('version'), result);
  };

  let checkClusterNodeEngine = function() {
    // this will just redirect to a new endpoint
    let result = arango.GET('/_admin/clusterNodeEngine?ServerID=' + servers[0]);
    assertEqual(308, result.code);
    
    result = arango.GET('/_admin/cluster/nodeEngine?ServerID=' + servers[0]);
    assertTrue(result.hasOwnProperty('name'), result);
  };

  let checkClusterNodeStats = function() {
    // this will just redirect to a new endpoint
    let result = arango.GET('/_admin/clusterNodeStats?ServerID=' + servers[0]);
    assertEqual(308, result.code);
    
    result = arango.GET('/_admin/cluster/nodeStatistics?ServerID=' + servers[0]);
    assertTrue(result.hasOwnProperty('time'), result);
    assertTrue(result.hasOwnProperty('system'), result);
  };

  return {

    setUpAll: function () {
      servers = Object.keys(arango.GET('/_admin/cluster/health').Health).filter(function(s) {
        return s.match(/^PRMR/);
      });

      assertTrue(servers.length > 0, servers);

      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }
      
      db._createDatabase('UnitTestsDatabase');
      db._useDatabase('UnitTestsDatabase');
      
      users.save(user1, "foobar");
      users.save(user2, "foobar");
      users.grantDatabase(user1, 'UnitTestsDatabase');
      users.grantCollection(user1, 'UnitTestsDatabase', "*");
      users.grantDatabase(user2, 'UnitTestsDatabase', 'ro');
      users.reload();
    },

    tearDownAll: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");
      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }

      try {
        db._dropDatabase('UnitTestsDatabase');
      } catch (err) {
      }
    },
    
    testClusterHealthRoot: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', 'root', '');
      checkClusterHealth();
    },
    
    testClusterHealthOtherDBRW: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, "foobar");
      checkClusterHealth();
    },
    
    testClusterHealthOtherDBRO: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, "foobar");
      checkClusterHealth();
    },
    
    testClusterNodeVersionRoot: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', 'root', '');
      checkClusterNodeVersion();
    },
    
    testClusterNodeVersionDBRW: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, "foobar");
      checkClusterNodeVersion();
    },
    
    testClusterNodeVersionDBRO: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, "foobar");
      checkClusterNodeVersion();
    },
    
    testClusterNodeEngineRoot: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', 'root', '');
      checkClusterNodeEngine();
    },
    
    testClusterNodeEngineOtherDBRW: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, "foobar");
      checkClusterNodeEngine();
    },
    
    testClusterNodeEngineOtherDBRO: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, "foobar");
      checkClusterNodeEngine();
    },
    
    testClusterNodeStatsRoot: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', 'root', '');
      checkClusterNodeStats();
    },
    
    testClusterNodeStatsOtherDBRW: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, "foobar");
      checkClusterNodeStats();
    },
    
    testClusterNodeStatsOtherDBRO: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, "foobar");
      checkClusterNodeStats();
    },
    
  };
}


jsunity.run(AuthSuite);

return jsunity.done();
