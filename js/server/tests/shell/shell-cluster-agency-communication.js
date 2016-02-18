/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertFalse, assertUndefined, assertNotUndefined*/

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
  var jsunity = require("jsunity");
  var _ = require("lodash");

  
  var createResult = function(prefixes, list) {
    'use strict';
    var prefix = prefixes.join("/") + "/";
    var res = {};
    _.each(list, function(v, k) {
      res[prefix + k] = v;
    });
    return res;
  };

  
  var Communication = require("@arangodb/cluster/agency-communication");
  var comm;
  var dummy;
  var ips;
  var agencyRoutes = {
    vision: "Vision",
    target: "Target",
    plan: "Plan",
    current: "Current",
    sync: "Sync",
    sub: {
      servers: "DBServers",
      databases: "Databases",
      coords: "Coordinators",
      colls: "Collections",
      mapToE: "MapIDToEndpoint",
      registered: "ServersRegistered",
      beat: "ServerStates",
      interval: "HeartbeatIntervalMs"
    }
  };
  var resetToDefault = function() {
    'use strict';
    var dbServers = {
      "pavel": "sandro",
      "paul": "sally",
      "patricia": "sandra"
    };
    var coordinators = {
      "cindy": "none",
      "carlos": "none",
      "charly": "none"
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
        "98213": {name: "_graphs"},
        "87123": vInfo,
        "89123": {name: "e"}
      },
      a_db: {
        "11235": {name: "s"},
        "6512": {name: "a"},
        "123": {name: "d"}
      },
      current: {
        _system: {
          "98213": {
            "sg1": {},
            "sg2": {},
            "sg3": {}
          },
          "87123": {
            "sv1": {},
            "sv2": {},
            "sv3": {}
          },
          "89123": {
            "se1": {},
            "se2": {},
            "se3": {}
          }
        },
        a_db: {
          "11235": {
            "s01": {},
            "s02": {},
            "s03": {}
          },
          "6512": {
            "s11": {},
            "s12": {},
            "s13": {}
          },
          "123": {
            "s21": {},
            "s22": {},
            "s23": {}
          }
        }
      }
    };
    ips = {
      "pavel": {endpoint: "tcp://192.168.0.1:8529"},
      "paul": {endpoint: "tcp://192.168.0.2:8529"},
      "patricia": {endpoint: "tcp://192.168.0.3:8529"},
      "sandro": {endpoint: "tcp://192.168.0.4:8529"},
      "sally": {endpoint: "tcp://192.168.0.5:8529"},
      "sandra": {endpoint: "tcp://192.168.0.6:8529"},
      "carlos": {endpoint: "tcp://192.168.1.1:8529"},
      "charly": {endpoint: "tcp://192.168.1.2:8529"},
      "cindy": {endpoint: "tcp://192.168.1.3:8529"}
    };
    var heartbeats = {
      pavel: {
        status: "SERVINGSYNC",
        time: (new Date()).toISOString()
      },
      paul: {
        status: "SERVINGSYNC",
        time: (new Date()).toISOString()
      },
      patricia: {
        status: "SERVINGASYNC",
        time: (new Date()).toISOString()
      },
      sandro: {
        status: "INSYNC",
        time: (new Date()).toISOString()
      },
      sandra: {
        status: "SYNCING",
        time: (new Date()).toISOString()
      },
      sally: {
        status: "INSYNC",
        time: (new Date()).toISOString()
      }
    };
    dummy = {};
    dummy.target = {};
    dummy.target.servers = createResult([agencyRoutes.target, agencyRoutes.sub.servers], dbServers);
    dummy.target.coordinators = createResult([agencyRoutes.target, agencyRoutes.sub.coords], coordinators);
    dummy.target.databases = databases;
    dummy.target.syscollections = createResult([agencyRoutes.target,
                                                agencyRoutes.sub.databases,
                                                agencyRoutes.sub.colls,
                                                "_system"], collections._system);

    dummy.target.acollections = createResult([agencyRoutes.target,
                                              agencyRoutes.sub.databases,
                                              agencyRoutes.sub.colls,
                                              "a_db"],
                                             collections.a_db);
    dummy.target.vInfo = vInfo;

    dummy.plan = {};
    dummy.plan.servers = createResult([agencyRoutes.plan, agencyRoutes.sub.servers], dbServers);
    dummy.plan.coordinators = createResult([agencyRoutes.plan, agencyRoutes.sub.coords], coordinators);
    dummy.plan.databases = databases;

    dummy.plan.syscollections = createResult([agencyRoutes.plan,
                                              agencyRoutes.sub.databases,
                                              agencyRoutes.sub.colls,
                                              "_system"],
                                             collections._system);

    dummy.plan.acollections = createResult([agencyRoutes.plan,
                                            agencyRoutes.sub.databases,
                                            agencyRoutes.sub.colls,
                                            "a_db"],
                                           collections.a_db);
    dummy.plan.vInfo = vInfo;

    dummy.current = {};
    dummy.current.servers = createResult([agencyRoutes.current, agencyRoutes.sub.servers], dbServers);
    dummy.current.coordinators = createResult([agencyRoutes.current, agencyRoutes.sub.coords], coordinators);
    dummy.current.registered = createResult([agencyRoutes.current, agencyRoutes.sub.registered], ips);
    dummy.current.databases = databases;
    dummy.current.syscollections = createResult([agencyRoutes.current,
                                                 agencyRoutes.sub.databases,
                                                 agencyRoutes.sub.colls,
                                                 "_system"],
                                                collections.current._system);

    dummy.current.acollections = createResult([agencyRoutes.current,
                                               agencyRoutes.sub.databases,
                                               agencyRoutes.sub.colls,
                                               "a_db"],
                                              collections.current.a_db);
    dummy.current.vInfo = vInfo;

    dummy.sync = {};
    dummy.sync.heartbeats = heartbeats;
    dummy.sync.heartbeats = createResult([agencyRoutes.registered, agencyRoutes.sub.beat], heartbeats);
    dummy.sync.interval = "1000";
  };
  var setup = function() {
    'use strict';
    resetToDefault();
    comm = new Communication.Communication();
  };
  var teardown = function() {};
  var agencyMock = {
    get: function(route, recursive) {
      'use strict';
      var parts = route.split("/");
      var res;
      var returnResult = function(base) {
        if (parts[1] === agencyRoutes.sub.servers && recursive) {
          return base.servers;
        }
        if (parts[1] === agencyRoutes.sub.coords && recursive) {
          return base.coordinators;
        }
        if (parts[1] === agencyRoutes.sub.colls) {
          if (recursive) {
            if (parts[2] === "_system") {
              return base.syscollections;
            }
            if (parts[2] === "a_db") {
              return base.acollections;
            }
            if (parts[2] === "b_db") {
              return {};
            }
            if (parts[2] === "z_db") {
              return {};
            }
          } else {
            if (parts[2] === "_system" && parts[3] === "87123") {
              var internalResult = {};
              internalResult[route] = base.vInfo;
              return internalResult;
            }
          }
        }
      };
      switch (parts[0]) {
        case agencyRoutes.target:
          if (parts[1] === agencyRoutes.sub.mapToE) {
            res = ips[parts[2]];
            if (!res) {
              fail("Could not find IP of: " + parts[2]);
            }
            return res;
          }
          res = returnResult(dummy.target);
          if (res) {
            return res;
          }
          break;
        case agencyRoutes.plan:
          res = returnResult(dummy.plan);
          if (res) {
            return res;
          }
          break;
        case agencyRoutes.current:
          res = returnResult(dummy.current);
          if (res) {
            return res;
          }
          if (parts[1] === agencyRoutes.sub.registered && recursive) {
            return dummy.current.registered;
          }
          break;
        case agencyRoutes.sync:
          if (parts[1] === agencyRoutes.sub.beat && recursive) {
            return dummy.sync.heartbeats;
          }
          if (parts[1] === agencyRoutes.sub.interval) {
            res = {};
            res[route] = dummy.sync.interval;
            return res;
          }
          break;
        default:
          fail("Requested route: GET " + route);
      }
      fail("Requested route: GET " + route);
    },
    list: function(route, recursive, flat) {
      'use strict';
      var parts = route.split("/");
      var returnResult = function(route) {
        if (parts[1] === agencyRoutes.sub.databases) {
          return route.databases;
        }
        if (parts[1] === agencyRoutes.sub.colls) {
          return _.map(route.databases, function(d) {
            return route + "/" + d;
          });
        }
        if (parts[1] === agencyRoutes.sub.coords) {
          return _.keys(route.coordinators);
        }
      };
      if (!flat) {
        fail("List is not requested flat");
      }
      var res;
      switch (parts[0]) {
        case agencyRoutes.target:
          res = returnResult(dummy.target);
          if (res) {
            return res;
          }
          break;
        case agencyRoutes.plan:
          res = returnResult(dummy.plan);
          if (res) {
            return res;
          }
          break;
        case agencyRoutes.current:
          res = returnResult(dummy.current);
          if (res) {
            return res;
          }
          break;
        default:
          fail("Requested route: LIST " + route);
      }
    }
  };

  Communication._createAgency = function() {
    'use strict';
    return agencyMock;
  };

  
  function runTargetTests(test) {
    'use strict';

  
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
    }

  
    function CoordinatorSuite() {
      var targetCoordinators;

      return {
        setUp: function() {
          setup();
          targetCoordinators = comm.target.Coordinators();
        },
        tearDown: teardown,

        testGetCoordinatorList: function() {
          var list = {
            "carlos": {"role": "primary"},
            "charly": {"role": "primary"},
            "cindy": {"role": "primary"}
          };
          assertEqual(targetCoordinators.getList(), list);
        },

        testAddCoordinator: function() {
          var name = "carol";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.coords, name].join("/"));
            assertEqual(value, "none");
            dummy.target.coordinators[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(targetCoordinators.add(name), "Failed to insert a new coordinator.");
          assertTrue(wasCalled, "Agency has not been informed to insert coordinator.");
          var list = {
            "carlos": {"role": "primary"},
            "charly": {"role": "primary"},
            "cindy": {"role": "primary"}
          };
          list[name] = {"role": "primary"};
          assertEqual(targetCoordinators.getList(), list);
        },

        testRemoveCoordinator: function() {
          var name = "cindy";
          var delWasCalled = false;
          agencyMock.remove = function(route) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.coords, name].join("/"));
            assertFalse(delWasCalled, "Delete has been called multiple times");
            delete dummy.target.coordinators[route];
            delWasCalled = true;
            return true;
          };
          assertTrue(targetCoordinators.remove(name), "Failed to remove a coordinator.");
          assertTrue(delWasCalled, "Agency has not been informed to remove a coordinator.");
          var list = {
            "carlos": {"role": "primary"},
            "charly": {"role": "primary"},
          };
          assertEqual(targetCoordinators.getList(), list);
        }
      };
    }

  
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
          assertEqual(sysdb.collection("v").info(), dummy.target.vInfo);
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
    }

    test.run(DBServersSuite);
    test.run(CoordinatorSuite);
    test.run(DataSuite);
  }

  
  function runPlanTests(test) {
    'use strict';

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
    }

    function CoordinatorSuite() {
      var coordinators;

      return {
        setUp: function() {
          setup();
          coordinators = comm.plan.Coordinators();
        },
        tearDown: teardown,

        testGetCoordinatorList: function() {
          var list = {
            "carlos": {"role": "primary"},
            "charly": {"role": "primary"},
            "cindy": {"role": "primary"}
          };
          assertEqual(coordinators.getList(), list);
        }
      };
    }

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
          assertEqual(sysdb.collection("v").info(), dummy.target.vInfo);
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
    }

    test.run(DBServersSuite);
    test.run(CoordinatorSuite);
    test.run(DataSuite);
  }

  
  function runCurrentTests(test) {
    'use strict';
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
              address: "192.168.0.1:8529",
              protocol: "http"
            },
            paul: {
              role: "primary",
              secondary: "sally",
              address: "192.168.0.2:8529",
              protocol: "http"
            },
            patricia: {
              role: "primary",
              secondary: "sandra",
              address: "192.168.0.3:8529",
              protocol: "http"
            },
            sandro: {
              role: "secondary",
              address: "192.168.0.4:8529",
              protocol: "http"
            },
            sally: {
              role: "secondary",
              address: "192.168.0.5:8529",
              protocol: "http"
            },
            sandra: {
              role: "secondary",
              address: "192.168.0.6:8529",
              protocol: "http"
            }
          };
          var res = targetServers.getList();
          assertEqual(res, expected);
        }

      };

    }

    function CoordinatorSuite() {

      return {
        setUp: setup,
        tearDown: teardown,

        testGetServerList: function() {
          var expected = {
            "carlos": {
              "address": "192.168.1.1:8529",
              "protocol": "http"
            },
            "charly": {
              "address": "192.168.1.2:8529",
              "protocol": "http"
            },
            "cindy": {
              "address": "192.168.1.3:8529",
              "protocol": "http"
            }
          };
          var res = comm.current.Coordinators().getList();
          assertEqual(res, expected);
        }
      };

    }

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
          assertEqual(sysdb.collection("v").info(), dummy.target.vInfo);
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
    }

    test.run(DBServersSuite);
    test.run(CoordinatorSuite);
    test.run(DataSuite);
  }

  
  function runSyncTests(test) {
    'use strict';
    function HeartbeatSuite() {
      var beats;

      return {
        setUp: function() {
          setup();
          beats = comm.sync.Heartbeats();
        },
        tearDown: teardown,

        testGetHeartbeats: function() {
          assertEqual(beats.list(), dummy.sync.heartbeats);
        },

        testGetInactiveServers: function() {
          assertEqual(beats.getInactive(), []);
        },

        testGetServingServers: function() {
          assertEqual(beats.getServing(), ["pavel", "paul", "patricia"].sort());
        },

        testGetInSyncPairs: function() {
          assertEqual(beats.getInSync(), [
            "pavel", "sandro",
            "paul", "sally"
          ].sort());
        },

        testGetOutOfSyncPairs: function() {
          assertEqual(beats.getOutSync(), ["patricia", "sandra"].sort());
        },

        testGetNoBeatIfAllWork: function() {
          assertEqual(beats.noBeat(), []);
        },

        testGetNoBeatIfOneFails: function() {
          // Rewrite Beat
          var now = new Date(),
              old = new Date(now - 5 * 60 * 1000),
              route = [
                agencyRoutes.registered,
                agencyRoutes.sub.beat,
                "pavel"
              ].join("/");
          dummy.sync.heartbeats[route].time = old.toISOString();
          assertEqual(beats.noBeat(), ["pavel"]);
        }
      };
    }

    function ProblemsSuite() {
      // TODO Not yet fully defined
      return {
        setUp: setup,
        tearDown: teardown

      };
    }

    test.run(HeartbeatSuite);
    test.run(ProblemsSuite);

  }


  
  function runHighLevelTests(test) {
    'use strict';
    function ConfigureSuite() {

      var targetServers;
      var targetCoordinators;

      return {
        setUp: function() {
          setup();
          targetServers = comm.target.DBServers();
          targetCoordinators = comm.target.Coordinators();
        },
        tearDown: teardown,

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
          assertTrue(comm.addPrimary(name), "Failed to insert a new primary");
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
          assertTrue(comm.addPrimary(name), "Failed to insert a new primary");
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
          assertTrue(comm.addSecondary(secName, name), "Failed to insert a new secondary");
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
          assertTrue(comm.addPair(name, secName), "Failed to insert a new primary/secondary pair");
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
          assertTrue(comm.removeServer(name), "Failed to remove a primary server");
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
          assertTrue(comm.removeServer(name), "Failed to remove a secondary server.");
          assertTrue(wasCalled, "Agency has not been informed to update the primary server.");
          var newList = targetServers.getList();
          assertUndefined(newList[name]);
          assertNotUndefined(newList[pName]);
          assertEqual(newList[pName].role, "primary");
          assertUndefined(newList[pName].secondary);
        },

        testAddCoordinator: function() {
          var name = "carol";
          var wasCalled = false;
          agencyMock.set = function(route, value) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.coords, name].join("/"));
            assertEqual(value, "none");
            dummy.target.coordinators[route] = value;
            wasCalled = true;
            return true;
          };
          assertTrue(comm.addCoordinator(name), "Failed to insert a new coordinator.");
          assertTrue(wasCalled, "Agency has not been informed to insert coordinator.");
          var list = {
            "carlos": {"role": "primary"},
            "charly": {"role": "primary"},
            "cindy": {"role": "primary"}
          };
          list[name] = {"role": "primary"};
          assertEqual(targetCoordinators.getList(), list);
        },

        testRemoveCoordinator: function() {
          var name = "cindy";
          var delWasCalled = false;
          agencyMock.remove = function(route) {
            assertEqual(route, [agencyRoutes.target, agencyRoutes.sub.coords, name].join("/"));
            assertFalse(delWasCalled, "Delete has been called multiple times");
            delete dummy.target.coordinators[route];
            delWasCalled = true;
            return true;
          };
          assertTrue(comm.removeServer(name), "Failed to remove a coordinator.");
          assertTrue(delWasCalled, "Agency has not been informed to remove a coordinator.");
          var list = {
            "carlos": {"role": "primary"},
            "charly": {"role": "primary"},
          };
          assertEqual(targetCoordinators.getList(), list);
        }
      };
    }

    function DifferenceSuite() {
      var planMissingDB = [{
        name: "pavel",
        role: "primary",
        address: ips.pavel.endpoint.split("://")[1],
        protocol: "http"
      }, {
        name: "sandra",
        role: "secondary",
        address: ips.sandra.endpoint.split("://")[1],
        protocol: "http"
      }];
      var planMissingCoords = [{
        name: "carlos",
        role: "primary",
        address: ips.carlos.endpoint.split("://")[1],
        protocol: "http"
      }];
      var currentMissingDB = [{
        name: "patricia",
        role: "primary",
        address: ips.patricia.endpoint.split("://")[1],
        protocol: "http"
      }];
      var currentMissingCoords = [{
        name: "charly",
        role: "primary",
        address: ips.charly.endpoint.split("://")[1],
        protocol: "http"
      }];
      var modified = "sandro";
      var modplan = "none";
      var lonelyPrimary = "patricia";

      return {
        setUp: function() {
          _.each(planMissingDB, function(obj) {
            var d = obj.name;
            delete dummy.plan.servers[[agencyRoutes.plan, agencyRoutes.sub.servers, d].join("/")];
            delete dummy.current.servers[[agencyRoutes.current, agencyRoutes.sub.servers, d].join("/")];
          });
          dummy.plan.servers[[agencyRoutes.plan, agencyRoutes.sub.servers, lonelyPrimary].join("/")] = "none";
          dummy.current.servers[[agencyRoutes.current, agencyRoutes.sub.servers, lonelyPrimary].join("/")] = "none";

          _.each(planMissingCoords, function(obj) {
            var d = obj.name;
            delete dummy.plan.coordinators[[agencyRoutes.plan, agencyRoutes.sub.coords, d].join("/")];
            delete dummy.current.coordinators[[agencyRoutes.current, agencyRoutes.sub.coords, d].join("/")];
          });
          _.each(currentMissingDB, function(obj) {
            var d = obj.name;
            delete dummy.current.servers[[agencyRoutes.current, agencyRoutes.sub.servers, d].join("/")];
          });
          _.each(currentMissingCoords, function(obj) {
            var d = obj.name;
            delete dummy.current.coordinators[[agencyRoutes.current, agencyRoutes.sub.coords, d].join("/")];
          });
          dummy.plan.servers[[agencyRoutes.plan, agencyRoutes.sub.servers, modified].join("/")] = modplan;
          dummy.current.servers[[agencyRoutes.current, agencyRoutes.sub.servers, modified].join("/")] = modplan;

          comm = new Communication.Communication();
        },
        tearDown: teardown,

        testDifferencePlanDBServers: function() {
          var diff = comm.diff.plan.DBServers();
          assertEqual(diff.missing, planMissingDB);
          assertEqual(diff.difference[modified], {
            target: {role: "secondary"},
            plan: {role: "primary"}
          });
          assertEqual(diff.difference[lonelyPrimary], {
            target: {
              role: "primary",
              secondary: "sandra"
            },
            plan: {role: "primary"}
          });
        },

        testDifferencePlanCoordinators: function() {
          var diff = comm.diff.plan.Coordinators();
          assertEqual(diff.missing, planMissingCoords);
          assertEqual(diff.difference, {});
        },

        testDifferenceCurrentDBServers: function() {
          var diff = comm.diff.current.DBServers();
          assertEqual(diff.missing, currentMissingDB);
        },

        testDifferenceCurrentCoordinators: function() {
          var diff = comm.diff.current.Coordinators();
          assertEqual(diff.missing, currentMissingCoords);
        }
      };
    }

    test.run(ConfigureSuite);
    test.run(DifferenceSuite);

  }

  
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief executes the test suites
  ////////////////////////////////////////////////////////////////////////////////
  runTargetTests(jsunity);
  runPlanTests(jsunity);
  runCurrentTests(jsunity);
  runSyncTests(jsunity);
  runHighLevelTests(jsunity);

  return jsunity.done();

  