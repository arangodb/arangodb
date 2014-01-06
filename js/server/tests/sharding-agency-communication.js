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
    fail: "Fail",
    sub: {
      servers:"DBServers",
      coords: "Coordinators",
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

    dummy.plan = {};
    dummy.plan.servers = createResult([agencyRoutes.plan, agencyRoutes.sub.servers], dbServers);

    dummy.current = {};
    dummy.current.servers = createResult([agencyRoutes.current, agencyRoutes.sub.servers], dbServers);
    dummy.current.coordinators = createResult([agencyRoutes.current, agencyRoutes.sub.coords], coordinators);
    dummy.current.registered = createResult([agencyRoutes.current, agencyRoutes.sub.registered], ips);
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
          break;
        case agencyRoutes.plan:
          if (parts[1] === agencyRoutes.sub.servers && recursive) {
            return dummy.plan.servers;
          }
          break;
        case agencyRoutes.current:
          if (parts[1] === agencyRoutes.sub.servers && recursive) {
            return dummy.current.servers;
          }
          if (parts[1] === agencyRoutes.sub.registered && recursive) {
            return dummy.current.registered;
          }
          break;
        default:
          fail();
      }
    },
    list: function(route) {
      var parts = route.split("/");
      switch (parts[0]) {
        case agencyRoutes.target:
          if (parts[1] === "Collections") {
            return dummy.target.databases;
          }
          if (parts[1] === agencyRoutes.sub.coords) {
            return _.map(
              _.keys(dummy.target.coordinators),
              function(k) {
                var splits = k.split("/");
                return splits[splits.length-1];
              }
            );
          }
          break;
        case agencyRoutes.current:
          if (parts[1] === agencyRoutes.sub.coords) {
            return _.map(
              _.keys(dummy.current.coordinators),
              function(k) {
                var splits = k.split("/");
                return splits[splits.length-1];
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
     
      return {
        setUp: setup,
        tearDown: teardown,

        testGetDatabaseList: function() {
          var list = [
            "_system",
            "a_db",
            "b_db",
            "z_db"
          ].sort();
          var targetDBs = comm.target.Databases();
          assertEqual(targetDBs.getList(), list);
        },

        testGetCollectionListForDatabase: function() {
          /*
          var list = [
            "_graphs",
            "v",
            "e"
          ].sort();
          var targetColls = comm.target._system();
          assertEqual(targetColls.getList(), list);
          */
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

    test.run(DBServersSuite);
    test.run(CoordinatorSuite);
  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                              main
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief executes the test suites
  ////////////////////////////////////////////////////////////////////////////////

  runTargetTests(jsunity);
  runCurrentTests(jsunity);

  return jsunity.done();

  // Local Variables:
  // mode: outline-minor
  // outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
  // End:
}());
