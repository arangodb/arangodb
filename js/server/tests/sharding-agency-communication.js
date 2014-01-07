/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true */
/*global require*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test the agency communication layer
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
/// @author Michael Hackstein
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
(function() {
  "use strict";
  var jsunity = require("jsunity");
  var internal = require("internal");
  var _ = require("underscore");

  // -----------------------------------------------------------------------------
  // --SECTION--                                              agency-result-helper
  // -----------------------------------------------------------------------------

  var createResult = function(prefixes, list) {
    var prefix = prefixes.join("/") + "/";
    var res = {};
    _.each(list, function(v, k) {
      res[prefix + k] = v; 
    });
    return res;
  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     global values
  // -----------------------------------------------------------------------------
  var Communication = require("org/arangodb/sharding/agency-communication");
  var comm;
  var dummy;
  var agencyRoutes = {
    vision: "Vision",
    target: "Target",
    plan: "Plan",
    current: "Current",
    fail: "Sync",
    sub: {
      servers:"DBServers",
      coords: "Coordinators",
      colls: "Collections",
      registered: "ServersRegistered"
    }
  };
  var resetToDefault = function() {
    var dbServers = {
      "pavel": "sandro",
      "paul": "sally",
      "patricia": "sandra"
    };
    var coordinators = {
      "cindy": 2,
      "carlos": true,
      "charly": "alice"
    };
    var databases = ["_system", "z_db", "a_db", "b_db"];
    var vInfo = {
      status: "loaded",
      shardKeys: ["_key"],
      name: "v",
      shards: {
        v1: "pavel",
        v2: "paul",
        v3: "patricia", 
        v4: "pavel",
        v5: "patricia",
        v6: "pavel"
      }
    };
    var collections = {
      _system: {
        "98213": JSON.stringify({name: "_graphs"}),
        "87123": JSON.stringify(vInfo),
        "89123": JSON.stringify({name: "e"})
      },
      a_db: {
        "11235": JSON.stringify({name: "s"}),
        "6512": JSON.stringify({name: "a"}),
        "123": JSON.stringify({name: "d"})
      }
    };
    var ips = {
      "pavel": "tcp://192.168.0.1:8529",
      "paul": "tcp://192.168.0.2:8529",
      "patricia": "tcp://192.168.0.3:8529",
      "sandro": "tcp://192.168.0.4:8529",
      "sally": "tcp://192.168.0.5:8529",
      "sandra": "tcp://192.168.0.6:8529",
      "carlos": "tcp://192.168.1.1:8529",
      "charly": "tcp://192.168.1.2:8529",
      "cindy": "tcp://192.168.1.3:8529"
    };
    dummy = {};
    dummy.target = {};
    dummy.target.servers = createResult([agencyRoutes.target, agencyRoutes.sub.servers], dbServers);
    dummy.target.coordinators = createResult([agencyRoutes.target, agencyRoutes.sub.coords], coordinators);
    dummy.target.databases = databases;
    dummy.target.syscollections = createResult([agencyRoutes.target, agencyRoutes.sub.databases, agencyRoutes.sub.colls, "_system"], collections._system);
    dummy.target.acollections = createResult([agencyRoutes.target, agencyRoutes.sub.databases, agencyRoutes.sub.colls, "a_db"], collections.a_db);
    dummy.target.vInfo = JSON.stringify(vInfo);

    dummy.plan = {};
    dummy.plan.servers = createResult([agencyRoutes.plan, agencyRoutes.sub.servers], dbServers);
    dummy.plan.coordinators = createResult([agencyRoutes.plan, agencyRoutes.sub.coords], coordinators);
    dummy.plan.databases = databases;
    dummy.plan.syscollections = createResult([agencyRoutes.plan, agencyRoutes.sub.databases, agencyRoutes.sub.colls, "_system"], collections._system);
    dummy.plan.acollections = createResult([agencyRoutes.plan, agencyRoutes.sub.databases, agencyRoutes.sub.colls, "a_db"], collections.a_db);
    dummy.plan.vInfo = JSON.stringify(vInfo);

    dummy.current = {};
    dummy.current.servers = createResult([agencyRoutes.current, agencyRoutes.sub.servers], dbServers);
    dummy.current.coordinators = createResult([agencyRoutes.current, agencyRoutes.sub.coords], coordinators);
    dummy.current.registered = createResult([agencyRoutes.current, agencyRoutes.sub.registered], ips);
    dummy.current.databases = databases;
    dummy.current.syscollections = createResult([agencyRoutes.current, agencyRoutes.sub.databases, agencyRoutes.sub.colls, "_system"], collections._system);
    dummy.current.acollections = createResult([agencyRoutes.current, agencyRoutes.sub.databases, agencyRoutes.sub.colls, "a_db"], collections.a_db);
    dummy.current.vInfo = JSON.stringify(vInfo);
  };
  var setup = function() {
    resetToDefault();
    comm = new Communication.Communication();
  };
  var teardown = function() {};
  var agencyMock = {
    get: function(route, recursive) {
      var parts = route.split("/");
      switch (parts[0]) {
        case agencyRoutes.target:
          if (parts[1] === agencyRoutes.sub.servers && recursive) {
            return dummy.target.servers;
          }
          if (parts[1] === agencyRoutes.sub.colls) {
            if (recursive) {
              if (parts[2] === "_system") {
                return dummy.target.syscollections;
              }
              if (parts[2] === "a_db") {
                return dummy.target.acollections;
              }
              if (parts[2] === "b_db") {
                return {};
              }
              if (parts[2] === "z_db") {
                return {};
              }
            } else {
              if (parts[2] === "_system" && parts[3] === "87123") {
                return dummy.target.vInfo;
              }
            }
          }
          break;
        case agencyRoutes.plan:
          if (parts[1] === agencyRoutes.sub.servers && recursive) {
            return dummy.plan.servers;
          }
          if (parts[1] === agencyRoutes.sub.colls) {
            if (recursive) {
              if (parts[2] === "_system") {
                return dummy.plan.syscollections;
              }
              if (parts[2] === "a_db") {
                return dummy.plan.acollections;
              }
              if (parts[2] === "b_db") {
                return {};
              }
              if (parts[2] === "z_db") {
                return {};
              }
            } else {
              if (parts[2] === "_system" && parts[3] === "87123") {
                return dummy.plan.vInfo;
              }
            }
          }
          break;
        case agencyRoutes.current:
          if (parts[1] === agencyRoutes.sub.servers && recursive) {
            return dummy.current.servers;
          }
          if (parts[1] === agencyRoutes.sub.colls) {
            if (recursive) {
              if (parts[2] === "_system") {
                return dummy.current.syscollections;
              }
              if (parts[2] === "a_db") {
                return dummy.current.acollections;
              }
              if (parts[2] === "b_db") {
                return {};
              }
              if (parts[2] === "z_db") {
                return {};
              }
            } else {
              if (parts[2] === "_system" && parts[3] === "87123") {
                return dummy.current.vInfo;
              }
            }
          }
          if (parts[1] === agencyRoutes.sub.registered && recursive) {
            return dummy.current.registered;
          }
          break;
        default:
          fail();
      }
      fail();
    },
    list: function(route) {
      var parts = route.split("/");
      switch (parts[0]) {
        case agencyRoutes.target:
          if (parts[1] === agencyRoutes.sub.colls) {
            return dummy.target.databases;
          }
          if (parts[1] === agencyRoutes.sub.coords) {
            return _.map(
              _.keys(dummy.target.coordinators),
              function(k) {
                var splits = k.split("/");
                return splits[splits.length - 1];
              }
            );
          }
          break;
        case agencyRoutes.plan:
          if (parts[1] === agencyRoutes.sub.colls) {
            return dummy.plan.databases;
          }
          if (parts[1] === agencyRoutes.sub.coords) {
            return _.map(
              _.keys(dummy.plan.coordinators),
              function(k) {
                var splits = k.split("/");
                return splits[splits.length - 1];
              }
            );
          }
          break;
        case agencyRoutes.current:
          if (parts[1] === agencyRoutes.sub.colls) {
            return dummy.current.databases;
          }
          if (parts[1] === agencyRoutes.sub.coords) {
            return _.map(
              _.keys(dummy.current.coordinators),
              function(k) {
                var splits = k.split("/");
                return splits[splits.length - 1];
              }
            );
          }
          break;
        default:
          fail();
      }
    }
  };

  Communication._createAgency = function() {
    return agencyMock;
  };


  // -----------------------------------------------------------------------------
  // --SECTION--                                                            vision
  // -----------------------------------------------------------------------------
  function runVisionTests(test) {
   // Not yet defined and in use, changes are applied in Target directly 
  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                            target
  // -----------------------------------------------------------------------------

  function runTargetTests(test) {

  // -----------------------------------------------------------------------------
  // --SECTION--                                                         DBServers
  // -----------------------------------------------------------------------------

    function DBServersSuite() {
      var targetServers;

      return {
        setUp: function() {
          setup();
          targetServers = comm.target.DBServers();
        },
        tearDown: teardown,

        testGetServerList: function() {
          var expected = {
            pavel: {
              role: "primary",
              secondary: "sandro"
            },
            paul: {
              role: "primary",
              secondary: "sally"
            },
            patricia: {
              role: "primary",
              secondary: "sandra"
            },
            sandro: {
              role: "secondary"
            },
            sally: {
              role: "secondary"
            },
            sandra: {
              role: "secondary"
            }
          };
          var res = targetServers.getList();
          assertEqual(res, expected);
        },

        testAddNewPrimaryServer: function() {
          var name = "pancho";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.servers, name].join("/"));
            assertEqual(value, "none");
            dummy.target.servers[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(targetServers.addPrimary(name), "Failed to insert a new primary");
          assertTrue(wasCalled, "Agency has not been informed to insert primary.");

          var newList = targetServers.getList();
          assertNotUndefined(newList[name]);
          assertEqual(newList[name].role, "primary");
          assertUndefined(newList[name].secondary);
        },

        testAddNewSecondaryServer: function() {
          var name = "pancho";
          var secName = "samuel";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.servers, name].join("/"));
            assertEqual(value, "none");
            assertFalse(wasCalled, "Set has been called multiple times");
            dummy.target.servers[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(targetServers.addPrimary(name), "Failed to insert a new primary");
          assertTrue(wasCalled, "Agency has not been informed to insert primary.");
          wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.servers, name].join("/"));
            assertEqual(value, secName);
            assertFalse(wasCalled, "Set has been called multiple times");
            dummy.target.servers[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(targetServers.addSecondary(secName, name), "Failed to insert a new secondary");
          assertTrue(wasCalled, "Agency has not been informed to insert secondary.");

          var newList = targetServers.getList();
          assertNotUndefined(newList[name]);
          assertEqual(newList[name].role, "primary");
          assertEqual(newList[name].secondary, secName);
          assertNotUndefined(newList[secName]);
          assertEqual(newList[secName].role, "secondary");
        },

        testAddNewServerPair: function() {
          var name = "pancho";
          var secName = "samuel";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.servers, name].join("/"));
            assertEqual(value, secName);
            assertFalse(wasCalled, "Set has been called multiple times");
            dummy.target.servers[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(targetServers.addPair(name, secName), "Failed to insert a new primary/secondary pair");
          assertTrue(wasCalled, "Agency has not been informed to insert the new pair.");
          var newList = targetServers.getList();
          assertNotUndefined(newList[name]);
          assertEqual(newList[name].role, "primary");
          assertEqual(newList[name].secondary, secName);
          assertNotUndefined(newList[secName]);
          assertEqual(newList[secName].role, "secondary");
        },

        testRemovePrimaryServer: function() {
          var name = "pavel";
          var secondaryName = "sandro";
          var setWasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.servers, secondaryName].join("/"));
            assertEqual(value, "none");
            assertFalse(setWasCalled, "Set has been called multiple times");
            dummy.target.servers[route] = value;
            setWasCalled = true;
            return true;
          };
          var delWasCalled = false;
          agencyMock.remove = function(route) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.servers, name].join("/"));
            assertFalse(delWasCalled, "Delete has been called multiple times");
            delete dummy.target.servers[route];
            delWasCalled = true;
            return true;
          };
          assertTrue(targetServers.removeServer(name), "Failed to remove a primary server");
          assertTrue(setWasCalled, "Agency has not been informed to replace the primary with the secondary.");
          assertTrue(delWasCalled, "Agency has not been informed to remove the primary/secondary pair.");

          var newList = targetServers.getList();
          assertUndefined(newList[name]);
          assertNotUndefined(newList[secondaryName]);
          assertEqual(newList[secondaryName].role, "primary");
          assertUndefined(newList[secondaryName].secondary);
        },

        testRemoveSecondaryServer: function() {
          var name = "sandro";
          var pName = "pavel";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.servers, pName].join("/"));
            assertEqual(value, "none");
            assertFalse(wasCalled, "Set has been called multiple times");
            dummy.target.servers[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(targetServers.removeServer(name), "Failed to remove a secondary server.");
          assertTrue(wasCalled, "Agency has not been informed to update the primary server.");
          var newList = targetServers.getList();
          assertUndefined(newList[name]);
          assertNotUndefined(newList[pName]);
          assertEqual(newList[pName].role, "primary");
          assertUndefined(newList[pName].secondary);
        }
      };
    };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                       Coordinator
  // -----------------------------------------------------------------------------

    function CoordinatorSuite() {
      var targetCoordinators;

      return {
        setUp: function() {
          setup();
          targetCoordinators = comm.target.Coordinators();
        },
        tearDown: teardown,

        testGetCoordinatorList: function() {
          var list = [
            "carlos",
            "charly",
            "cindy"
          ].sort();
          assertEqual(targetCoordinators.getList(), list);
        },

        testAddCoordinator: function() {
          var name = "carol";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.coords, name].join("/"));
            assertEqual(value, true);
            dummy.target.coordinators[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(targetCoordinators.add(name), "Failed to insert a new coordinator.");
          assertTrue(wasCalled, "Agency has not been informed to insert coordinator.");
          var list = [
            "carlos",
            "charly",
            "cindy",
            name
          ].sort();
          assertEqual(targetCoordinators.getList(), list);
        },

        testRemoveCoordinator: function() {
          var name = "cindy";
          var delWasCalled = false;
          agencyMock.remove = function(route) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.coords, name].join("/"));
            assertFalse(delWasCalled, "Delete has been called multiple times");
            delete dummy.target.coordinators[route];
            internal.print(route);
            internal.print(dummy.target.coordinators);
            delWasCalled = true;
            return true;
          };
          assertTrue(targetCoordinators.remove(name), "Failed to remove a coordinator.");
          assertTrue(delWasCalled, "Agency has not been informed to remove a coordinator.");
          var list = [
            "carlos",
            "charly"
          ].sort();
          assertEqual(targetCoordinators.getList(), list);
        }
      };
    };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                              data
  // -----------------------------------------------------------------------------

    function DataSuite() {
      var dbs;

      return {
        setUp: function() {
          setup();
          dbs = comm.target.Databases();
        },
        tearDown: teardown,

        testGetDatabaseList: function() {
          var list = [
            "_system",
            "a_db",
            "b_db",
            "z_db"
          ].sort();
          assertEqual(dbs.getList(), list);
        },

        testGetCollectionListForDatabase: function() {
          var syslist = [
            "_graphs",
            "v",
            "e"
          ].sort();
          var alist = [
            "s",
            "a",
            "d"
          ].sort();
          assertEqual(dbs.select("_system").getCollections(), syslist);
          assertEqual(dbs.select("a_db").getCollections(), alist);
        },

        testSelectNotExistingDatabase: function() {
          assertFalse(dbs.select("foxx"));
        },

        testGetCollectionMetaInfo: function() {
          var sysdb = dbs.select("_system");
          assertEqual(sysdb.collection("v").info(), JSON.parse(dummy.target.vInfo));
        },

        testGetResponsibilitiesForCollection: function() {
          var colV = dbs.select("_system").collection("v");
          var expected = {
            v1: "pavel",
            v2: "paul",
            v3: "patricia",
            v4: "pavel",
            v5: "patricia",
            v6: "pavel"
          };
          assertEqual(colV.getShards(), expected);
        },

        testGetShardsForServer: function() {
          var colV = dbs.select("_system").collection("v");
          assertEqual(colV.getShardsForServer("pavel"), ["v1", "v4", "v6"].sort()); 
          assertEqual(colV.getShardsForServer("paul"), ["v2"].sort()); 
          assertEqual(colV.getShardsForServer("patricia"), ["v3", "v5"].sort()); 
        },

        testGetServerForShard: function() {
          var colV = dbs.select("_system").collection("v");
          assertEqual(colV.getServerForShard("v1"), "pavel");
          assertEqual(colV.getServerForShard("v2"), "paul");
        },

        testMoveResponsibility: function() {
          var colV = dbs.select("_system").collection("v");
          var source = "pavel";
          var target = "paul";
          var shard = "v1";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.colls, "_system", "87123"].join("/"));
            require("internal").print(value);
            // TODO Check Object correctness
            dummy.target.vInfo = value;
            wasCalled = true;
            return true;
          };

          assertEqual(colV.getServerForShard(shard), source);
          assertTrue(colV.moveShard(shard, target), "Failed to move shard responsibility.");
          assertTrue(wasCalled, "Agency has not been informed to move shard..");
          assertEqual(colV.getServerForShard(shard), target);
        }
      };
    };

    test.run(DBServersSuite);
    test.run(CoordinatorSuite);
    test.run(DataSuite);
  };


  // -----------------------------------------------------------------------------
  // --SECTION--                                                              plan
  // -----------------------------------------------------------------------------

  function runPlanTests(test) {

    function DBServersSuite() {
      var servers;

      return {
        setUp: function() {
          setup();
          servers = comm.plan.DBServers();
        },
        tearDown: teardown,

        testGetServerList: function() {
          var expected = {
            pavel: {
              role: "primary",
              secondary: "sandro"
            },
            paul: {
              role: "primary",
              secondary: "sally"
            },
            patricia: {
              role: "primary",
              secondary: "sandra"
            },
            sandro: {
              role: "secondary"
            },
            sally: {
              role: "secondary"
            },
            sandra: {
              role: "secondary"
            }
          };
          var res = servers.getList();
          assertEqual(res, expected);
        }

      };
    };

    function CoordinatorSuite() {
      var coordinators;

      return {
        setUp: function() {
          setup();
          coordinators = comm.plan.Coordinators();
        },
        tearDown: teardown,

        testGetCoordinatorList: function() {
          var list = [
            "carlos",
            "charly",
            "cindy"
          ].sort();
          assertEqual(coordinators.getList(), list);
        }
      };
    };

    function DataSuite() {
      var dbs;

      return {
        setUp: function() {
          setup();
          dbs = comm.plan.Databases();
        },
        tearDown: teardown,

        testGetDatabaseList: function() {
          var list = [
            "_system",
            "a_db",
            "b_db",
            "z_db"
          ].sort();
          assertEqual(dbs.getList(), list);
        },

        testGetCollectionListForDatabase: function() {
          var syslist = [
            "_graphs",
            "v",
            "e"
          ].sort();
          var alist = [
            "s",
            "a",
            "d"
          ].sort();
          assertEqual(dbs.select("_system").getCollections(), syslist);
          assertEqual(dbs.select("a_db").getCollections(), alist);
        },

        testSelectNotExistingDatabase: function() {
          assertFalse(dbs.select("foxx"));
        },

        testGetCollectionMetaInfo: function() {
          var sysdb = dbs.select("_system");
          assertEqual(sysdb.collection("v").info(), JSON.parse(dummy.target.vInfo));
        },

        testGetResponsibilitiesForCollection: function() {
          var colV = dbs.select("_system").collection("v");
          var expected = {
            v1: "pavel",
            v2: "paul",
            v3: "patricia",
            v4: "pavel",
            v5: "patricia",
            v6: "pavel"
          };
          assertEqual(colV.getShards(), expected);
        },

        testGetShardsForServer: function() {
          var colV = dbs.select("_system").collection("v");
          assertEqual(colV.getShardsForServer("pavel"), ["v1", "v4", "v6"].sort()); 
          assertEqual(colV.getShardsForServer("paul"), ["v2"].sort()); 
          assertEqual(colV.getShardsForServer("patricia"), ["v3", "v5"].sort()); 
        },

        testGetServerForShard: function() {
          var colV = dbs.select("_system").collection("v");
          assertEqual(colV.getServerForShard("v1"), "pavel");
          assertEqual(colV.getServerForShard("v2"), "paul");
        }
      };
    };

    test.run(DBServersSuite);
    test.run(CoordinatorSuite);
    test.run(DataSuite);
  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                           current
  // -----------------------------------------------------------------------------

  function runCurrentTests(test) {

    function DBServersSuite() {
      var targetServers;

      return {
        setUp: function() {
          setup();
          targetServers = comm.current.DBServers();
        },
        tearDown: teardown,

        testGetServerList: function() {
          var expected = {
            pavel: {
              role: "primary",
              secondary: "sandro",
              address: "tcp://192.168.0.1:8529"
            },
            paul: {
              role: "primary",
              secondary: "sally",
              address: "tcp://192.168.0.2:8529"
            },
            patricia: {
              role: "primary",
              secondary: "sandra",
              address: "tcp://192.168.0.3:8529"
            },
            sandro: {
              role: "secondary",
              address: "tcp://192.168.0.4:8529"
            },
            sally: {
              role: "secondary",
              address: "tcp://192.168.0.5:8529"
            },
            sandra: {
              role: "secondary",
              address: "tcp://192.168.0.6:8529"
            }
          };
          var res = targetServers.getList();
          assertEqual(res, expected);
        }

      };

    };

    function CoordinatorSuite() {

      return {
        setUp: setup,
        tearDown: teardown,

        testGetServerList: function() {
          var expected = {
            "carlos": "tcp://192.168.1.1:8529",
            "charly": "tcp://192.168.1.2:8529",
            "cindy": "tcp://192.168.1.3:8529"
          };
          var res = comm.current.Coordinators().getList();
          assertEqual(res, expected);
        }
      };

    };

    function DataSuite() {
      var dbs;

      return {
        setUp: function() {
          setup();
          dbs = comm.current.Databases();
        },
        tearDown: teardown,

        testGetDatabaseList: function() {
          var list = [
            "_system",
            "a_db",
            "b_db",
            "z_db"
          ].sort();
          assertEqual(dbs.getList(), list);
        },

        testGetCollectionListForDatabase: function() {
          var syslist = [
            "_graphs",
            "v",
            "e"
          ].sort();
          var alist = [
            "s",
            "a",
            "d"
          ].sort();
          assertEqual(dbs.select("_system").getCollections(), syslist);
          assertEqual(dbs.select("a_db").getCollections(), alist);
        },

        testSelectNotExistingDatabase: function() {
          assertFalse(dbs.select("foxx"));
        },

        testGetCollectionMetaInfo: function() {
          var sysdb = dbs.select("_system");
          assertEqual(sysdb.collection("v").info(), JSON.parse(dummy.target.vInfo));
        },

        testGetResponsibilitiesForCollection: function() {
          var colV = dbs.select("_system").collection("v");
          var expected = {
            v1: "pavel",
            v2: "paul",
            v3: "patricia",
            v4: "pavel",
            v5: "patricia",
            v6: "pavel"
          };
          assertEqual(colV.getShards(), expected);
        },

        testGetShardsForServer: function() {
          var colV = dbs.select("_system").collection("v");
          assertEqual(colV.getShardsForServer("pavel"), ["v1", "v4", "v6"].sort()); 
          assertEqual(colV.getShardsForServer("paul"), ["v2"].sort()); 
          assertEqual(colV.getShardsForServer("patricia"), ["v3", "v5"].sort()); 
        },

        testGetServerForShard: function() {
          var colV = dbs.select("_system").collection("v");
          assertEqual(colV.getServerForShard("v1"), "pavel");
          assertEqual(colV.getServerForShard("v2"), "paul");
        }
      };
    };

    test.run(DBServersSuite);
    test.run(CoordinatorSuite);
    test.run(DataSuite);
  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                              Sync
  // -----------------------------------------------------------------------------

  function runSyncTests(test) {

  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                              main
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief executes the test suites
  ////////////////////////////////////////////////////////////////////////////////

  runVisionTests(jsunity);
  runTargetTests(jsunity);
  runPlanTests(jsunity);
  runCurrentTests(jsunity);
  runSyncTests(jsunity);

  return jsunity.done();

  // Local Variables:
  // mode: outline-minor
  // outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
  // End:
}());
