/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, arango */

//////////////////////////////////////////////////////////////////////////////
/// @brief test for soft shutdown of a coordinator
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2021 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
//////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const internal = require("internal");
const db = require("internal").db;
const time = internal.time;
const wait = internal.wait;
const {
  getCtrlCoordinators
} = require('@arangodb/test-helper');
const graph_module = require("@arangodb/general-graph");
const pregel = require("@arangodb/pregel");

function testSuitePregel() {
  'use strict';
  let oldLogLevel;
  let coordinator;

  // beware, pregel only uses float for pagerank,
  // which means up to 7 digits of precision.
  // Make this number too small and pagerank
  // will not converge.
  const EPS = 0.0001;
  const graphName = "UnitTest_pregel";
  const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

  function testAlgoStart(a, p) {
    let pid = pregel.start(a, graphName, p);
    assertTrue(typeof pid === "string");
    return pid;
  }

  return {

    /////////////////////////////////////////////////////////////////////////
    /// @brief set up
    /////////////////////////////////////////////////////////////////////////

    setUpAll : function() {
      oldLogLevel = arango.GET("/_admin/log/level").general;
      arango.PUT("/_admin/log/level", { general: "info" });

      let coordinators = getCtrlCoordinators();
      assertTrue(coordinators.length > 0);
      coordinator = coordinators[0];
    },

    tearDownAll : function () {
      // restore previous log level for "general" topic;
      arango.PUT("/_admin/log/level", { general: oldLogLevel });
    },

    setUp: function () {

      var guck = graph_module._list();
      var exists = guck.indexOf("demo") !== -1;
      if (exists || db.demo_v) {
        console.warn("Attention: graph_module gives:", guck, "or we have db.demo_v:", db.demo_v);
        return;
      }
      var graph = graph_module._create(graphName);
      db._create(vColl, { numberOfShards: 4 });
      graph._addVertexCollection(vColl);
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      var rel = graph_module._relation(eColl, [vColl], [vColl]);
      graph._extendEdgeDefinitions(rel);

      var vertices = db[vColl];
      var edges = db[eColl];


      vertices.insert([{ _key: 'A', sssp: 3, pagerank: 0.027645934 },
                       { _key: 'B', sssp: 2, pagerank: 0.3241496 },
                       { _key: 'C', sssp: 3, pagerank: 0.289220 },
                       { _key: 'D', sssp: 2, pagerank: 0.0329636 },
                       { _key: 'E', sssp: 1, pagerank: 0.0682141 },
                       { _key: 'F', sssp: 2, pagerank: 0.0329636 },
                       { _key: 'G', sssp: -1, pagerank: 0.0136363 },
                       { _key: 'H', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'I', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'J', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'K', sssp: 0, pagerank: 0.013636363 }]);

      edges.insert([{ _from: vColl + '/B', _to: vColl + '/C', vertex: 'B' },
                    { _from: vColl + '/C', _to: vColl + '/B', vertex: 'C' },
                    { _from: vColl + '/D', _to: vColl + '/A', vertex: 'D' },
                    { _from: vColl + '/D', _to: vColl + '/B', vertex: 'D' },
                    { _from: vColl + '/E', _to: vColl + '/B', vertex: 'E' },
                    { _from: vColl + '/E', _to: vColl + '/D', vertex: 'E' },
                    { _from: vColl + '/E', _to: vColl + '/F', vertex: 'E' },
                    { _from: vColl + '/F', _to: vColl + '/B', vertex: 'F' },
                    { _from: vColl + '/F', _to: vColl + '/E', vertex: 'F' },
                    { _from: vColl + '/G', _to: vColl + '/B', vertex: 'G' },
                    { _from: vColl + '/G', _to: vColl + '/E', vertex: 'G' },
                    { _from: vColl + '/H', _to: vColl + '/B', vertex: 'H' },
                    { _from: vColl + '/H', _to: vColl + '/E', vertex: 'H' },
                    { _from: vColl + '/I', _to: vColl + '/B', vertex: 'I' },
                    { _from: vColl + '/I', _to: vColl + '/E', vertex: 'I' },
                    { _from: vColl + '/J', _to: vColl + '/E', vertex: 'J' },
                    { _from: vColl + '/K', _to: vColl + '/E', vertex: 'K' }]);
    },

    //////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    //////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      // The soft shutdown initiated in the test
      // should just take care of shutting down
      if (coordinator.waitForInstanceShutdown(30)) {
        coordinator.restartOneInstance();
        coordinator.checkArangoConnection(10);
        
        graph_module._drop(graphName, true);
      } else {
        throw new Error("instance shutdown of coordinator failed!");
      }
    },

    testPageRank: function () {
      // Start a pregel run:
      // NOTE: EPS = 10E-7 might be intentional here to force pregel to run to 500 GSS which takes some
      // time rather than converging (which takes almost no time.
      let pid = testAlgoStart("pagerank", { threshold: 0.00000001, resultField: "result", store: true });

      console.warn("Started pregel run:", pid);

      // Now use soft shutdown API to shut coordinator down:
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      console.warn("Shutdown got:", res);
      assertEqual("OK", res, `expecting res = ${res} == "OK"`);
      let status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(1, status.pregelConductors, `expecting status.pregelConductors = ${status.pregelConductors} == 1`);

      // It should fail to create a new pregel run:
      let gotException = false;
      try {
        testAlgoStart("pagerank", { threshold: EPS, resultField: "result", store: true });
      } catch(e) {
        console.warn("Got exception for new run, good!", JSON.stringify(e));
        gotException = true;
      }
      assertTrue(gotException, `expect to catch exception for new pregel run.`);

      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing, `expect status.softShutdownOngoing == true`);

      let startTime = time();
      while (pregel.isBusy(pid)) {
        console.warn("Pregel still running...");
        wait(0.2);
        if (time() - startTime > 120) {
          assertTrue(false, "Pregel did not finish in time.");
        }
      }

      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
    },

  };
}

jsunity.run(testSuitePregel);
return jsunity.done();
