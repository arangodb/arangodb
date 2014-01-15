////////////////////////////////////////////////////////////////////////////////
/// @brief test the cluster helper functions
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

var cluster = require("org/arangodb/cluster");
var jsunity = require("jsunity");

var compareStringIds = function (l, r) {
  if (l.length != r.length) {
    return l.length - r.length < 0 ? -1 : 1;
  }

  // length is equal
  for (i = 0; i < l.length; ++i) {
    if (l[i] != r[i]) {
      return l[i] < r[i] ? -1 : 1;
    }
  }

  return 0;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                           cluster
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: cluster enabled
////////////////////////////////////////////////////////////////////////////////

function ClusterEnabledSuite () {
  var agency = ArangoAgency;
  var ss = ArangoServerState;
  var ci = ArangoClusterInfo;
  var oldPrefix = agency.prefix(true);
  var oldId = ss.id();
  var oldRole = ss.role();
      
  var cleanDirectories = function () {
    [ "Target", "Plan", "Current" ].forEach(function (d) {
      try {
        agency.remove(d, true);
      }
      catch (err) {
        // dir may not exist. this is not a problem
      }
    });
  };
  
  var initValues = function () {
    [ "Target", 
      "Plan", 
      "Current", 
      "Target/DBServers", 
      "Target/Coordinators", 
      "Target/Collections", 
      "Plan/DBServers", 
      "Plan/Coordinators", 
      "Plan/Collections", 
      "Current/ServersRegistered", 
      "Current/DBServers", 
      "Current/Coordinators", 
      "Current/Collections",
      "Current/ShardsCopied"
    ].forEach(function (d) {
      try {
        agency.createDirectory(d);
      }
      catch (err) {
      }
    });
  };

  return {
    setUp : function () {
      assertTrue(agency.setPrefix("UnitTestsAgency"));
      assertEqual("UnitTestsAgency", agency.prefix(true));
 
      cleanDirectories();
      initValues();
    },

    tearDown : function () {
      cleanDirectories();

      assertTrue(agency.setPrefix(oldPrefix));
      ss.setId(oldId);
      assertEqual(oldPrefix, agency.prefix(true));
      ss.setRole(oldRole);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test id
////////////////////////////////////////////////////////////////////////////////
    
    testId : function () {
      ss.setId("myself");
      assertEqual("myself", ss.id());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test address
////////////////////////////////////////////////////////////////////////////////
    
    testAddress : function () {
      assertTrue(agency.set("Target/MapIDToEndpoint/myself", "tcp://127.0.0.1:8529"));
      assertTrue(agency.set("Plan/Coordinators/myself", "none"));
      assertTrue(agency.set("Plan/DBServers/myself", "other"));
      ss.setId("myself");
      ss.flush();

      assertEqual("tcp://127.0.0.1:8529", ss.address());
      
      assertTrue(agency.set("Target/MapIDToEndpoint/myself", "tcp://127.0.0.1:8530"));
      ss.flush();
      
      assertEqual("tcp://127.0.0.1:8530", ss.address());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCluster
////////////////////////////////////////////////////////////////////////////////

    testIsCluster : function () {
      assertTrue(cluster.isCluster());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCoordinator
////////////////////////////////////////////////////////////////////////////////
    
    testIsCoordinatorTrue : function () {
      assertTrue(agency.set("Plan/Coordinators/myself", "none"));
      ss.setId("myself");
      ss.flush();

      assertTrue(cluster.isCoordinator());
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief test isCoordinator
////////////////////////////////////////////////////////////////////////////////
    
    testIsCoordinatorFalse : function () {
      assertTrue(agency.set("Plan/Coordinators/DBServers/myself", "other"));
      ss.setId("myself");
      ss.flush();

      assertFalse(cluster.isCoordinator());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test role
////////////////////////////////////////////////////////////////////////////////
    
    testRoleCoordinator : function () {
      assertTrue(agency.set("Plan/Coordinators/myself", "none"));
      ss.setId("myself");
      ss.flush();

      assertTrue(cluster.role() !== undefined);
      assertTrue(cluster.role() === "COORDINATOR");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test role
////////////////////////////////////////////////////////////////////////////////
    
    testRoleDBServer : function () {
      assertTrue(agency.set("Plan/DBServers/myself", "other"));
      ss.setId("myself");
      ss.flush();

      assertTrue(cluster.role() !== undefined);
      assertTrue(cluster.role() === "PRIMARY");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test status
////////////////////////////////////////////////////////////////////////////////
    
    testStatus : function () {
      assertTrue(cluster.status() !== undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCoordinatorRequest
////////////////////////////////////////////////////////////////////////////////
    
    testIsCoordinatorRequest : function () {
      assertFalse(cluster.isCoordinatorRequest(null));
      assertFalse(cluster.isCoordinatorRequest(undefined));
      assertFalse(cluster.isCoordinatorRequest({ }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { } }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { test: "" } }));
      assertTrue(cluster.isCoordinatorRequest({ headers: { "x-arango-coordinator": "abc" } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test doesDatabaseExist
////////////////////////////////////////////////////////////////////////////////

    testDoesDatabaseExist : function () {
      var database = {
        name: "test"
      };

      assertTrue(agency.set("Plan/Databases/" + database.name, database));
      assertTrue(agency.set("Current/DBServers/Foo", "Bar"));
      assertTrue(agency.set("Current/DBServers/Barz", "Bat"));
      assertTrue(agency.set("Current/Databases/test/Foo", database));
      assertTrue(agency.set("Current/Databases/test/Barz", database));

      assertTrue(ci.doesDatabaseExist("test"));

      assertFalse(ci.doesDatabaseExist("UnitTestsAgencyNonExisting"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test doesDatabaseExist
////////////////////////////////////////////////////////////////////////////////

    testDoesDatabaseExistNotReady : function () {
      var database = {
        name: "test"
      };

      assertTrue(agency.set("Plan/Databases/" + database.name, database));
      assertTrue(agency.set("Current/DBServers/Foo", "Bar"));
      assertTrue(agency.set("Current/DBServers/Barz", "Bat"));
      assertTrue(agency.set("Current/Databases/test/Foo", database));
      
      assertFalse(ci.doesDatabaseExist("test"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test getCollectionInfo
////////////////////////////////////////////////////////////////////////////////

    testGetCollectionInfo : function () {
      var collection = {
        id: "123",
        name: "mycollection",
        type: 2,
        status: 3, // LOADED 
        shardKeys: [ "_key" ],
        shards: { "s1" : "myself", "s2" : "other" }
      };
      assertTrue(agency.set("Current/Collections/test/" + collection.id, collection));

      var data = ci.getCollectionInfo("test", collection.id);

      assertEqual(collection.id, data.id);
      assertEqual(collection.name, data.name);
      assertEqual(collection.type, data.type);
      assertEqual(collection.status, data.status);
      assertEqual(collection.shardKeys, data.shardKeys);
      assertEqual(collection.shards, data.shards);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test getCollectionInfo
////////////////////////////////////////////////////////////////////////////////

    testGetCollectionInfo2 : function () {
      var collection = {
        id: "12345868390663",
        name: "mycollection_test",
        type: 3,
        status: 2, // LOADED 
        shardKeys: [ "_key", "a", "bc" ],
        shards: { "s1" : "myself", "s2" : "other", "s3" : "foo", "s4" : "bar" }
      };

      assertTrue(agency.set("Current/Collections/test/" + collection.id, collection));

      var data = ci.getCollectionInfo("test", collection.id);

      assertEqual(collection.id, data.id);
      assertEqual(collection.name, data.name);
      assertEqual(collection.type, data.type);
      assertEqual(collection.status, data.status);
      assertEqual(collection.shardKeys, data.shardKeys);
      assertEqual(collection.shards, data.shards);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test getResponsibleServer
////////////////////////////////////////////////////////////////////////////////

    testGetResponsibleServer : function () {
      var collection = {
        id: "12345868390663",
        name: "mycollection_test",
        type: 3,
        status: 2, // LOADED 
        shardKeys: [ "_key", "a", "bc" ],
        shards: { "s1" : "myself" }
      };

      assertTrue(agency.set("Current/Collections/test/" + collection.id, collection));
      ci.flush();

      assertEqual("myself", ci.getResponsibleServer("s1"));
      assertEqual("", ci.getResponsibleServer("s9999"));
     
      collection.shards = { s1: "other", s2: "myself" }; 
      assertTrue(agency.set("Current/Collections/test/" + collection.id, collection));
      ci.flush();

      assertEqual("other", ci.getResponsibleServer("s1"));
      assertEqual("myself", ci.getResponsibleServer("s2"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test getServerEndpoint
////////////////////////////////////////////////////////////////////////////////

    testGetServerEndpoint : function () {
      assertTrue(agency.set("Current/ServersRegistered/myself", "tcp://127.0.0.1:8529"));
      ci.flush();

      assertEqual("tcp://127.0.0.1:8529", ci.getServerEndpoint("myself"));
      
      assertTrue(agency.set("Current/ServersRegistered/myself", "tcp://127.0.0.1:8530"));
      ci.flush();

      assertEqual("tcp://127.0.0.1:8530", ci.getServerEndpoint("myself"));
      
      assertTrue(agency.remove("Current/ServersRegistered/myself"));
      assertTrue(agency.set("Current/ServersRegistered/other", "tcp://127.0.0.1:8529"));
      ci.flush();

      assertEqual("", ci.getServerEndpoint("myself"));
      assertEqual("tcp://127.0.0.1:8529", ci.getServerEndpoint("other"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uniqid
////////////////////////////////////////////////////////////////////////////////

    testUniqid : function () {
      var last = "0";

      for (var i = 0; i < 1005; ++i) {
        var id = ci.uniqid();
        assertEqual(-1, compareStringIds(last, id));
        last = id;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uniqid
////////////////////////////////////////////////////////////////////////////////

    testUniqidRange : function () {
      var last = "0";

      for (var i = 0; i < 100; ++i) {
        var id = ci.uniqid(10);
        assertEqual(-1, compareStringIds(last, id));
        last = id;
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: cluster disabled
////////////////////////////////////////////////////////////////////////////////

function ClusterDisabledSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCluster
////////////////////////////////////////////////////////////////////////////////

    testIsCluster : function () {
      assertFalse(cluster.isCluster());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test role
////////////////////////////////////////////////////////////////////////////////
    
    testRole : function () {
      assertTrue(cluster.role() === undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test status
////////////////////////////////////////////////////////////////////////////////
    
    testStatus : function () {
      assertTrue(cluster.status() === undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCoordinatorRequest
////////////////////////////////////////////////////////////////////////////////
    
    testIsCoordinatorRequest : function () {
      assertFalse(cluster.isCoordinatorRequest(null));
      assertFalse(cluster.isCoordinatorRequest(undefined));
      assertFalse(cluster.isCoordinatorRequest({ }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { } }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { test: "" } }));
      assertTrue(cluster.isCoordinatorRequest({ headers: { "x-arango-coordinator": "abc" } }));
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (cluster.isCluster()) {
  jsunity.run(ClusterEnabledSuite);
}
else {
  jsunity.run(ClusterDisabledSuite);
}

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

