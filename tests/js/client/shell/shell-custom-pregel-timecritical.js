/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Custom Pregel Tests
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2020 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// / @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

// ee/ce check + gm selection
const isEnterprise = require("internal").isEnterprise();
const isCluster = require("internal").isCluster();
const graphModule = isEnterprise? require("@arangodb/smart-graph") : require("@arangodb/general-graph");

// Air Modules
const vd = require("@arangodb/air/vertex-degrees");
const pr = require("@arangodb/air/pagerank");
const sssp = require("@arangodb/air/single-source-shortest-paths");
const scc = require("@arangodb/air/strongly-connected-components");
const ad = require("@arangodb/air/access-data");
const ga = require("@arangodb/air/global-accumulator-test");
const rw = require("@arangodb/air/random-walk");

function basicTestSuite() {
  'use strict';
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      // right now, no setup method is needed - might change
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      try {
        // Each testRunner will check if previous graph needs to be removed or not.
        // So here, we simply remove all available Graphs/SmartGraphs.
        // This could be improved in the future.
        const graphs = graphModule._list();
        graphs.forEach((graph) => {
          graphModule._drop(graph, true);
        });
      } catch (ignore) {
      }
    },

    testVertexDegrees: function () {
      if (!isEnterprise && isCluster) {
        // we do not want to test general graphs module here
        return;
      }
      assertTrue(vd.test());
    },

    testPageRank: function () {
      if (!isEnterprise && isCluster) {
        // we do not want to test general graphs module here
        return;
      }
      assertTrue(pr.test());
    },

    testSSSP: function () {
      if (!isEnterprise && isCluster) {
        // we do not want to test general graphs module here
        return;
      }
      assertTrue(sssp.test());
    },

    testSCC: function () {
      if (!isEnterprise && isCluster) {
        // we do not want to test general graphs module here
        return;
      }
      assertTrue(scc.test());
    },

    testAccessData: function () {
      if (!isEnterprise && isCluster) {
        // we do not want to test general graphs module here
        return;
      }
      assertTrue(ad.test());
    },

    testGlobalAccumulator: function () {
      if (!isEnterprise && isCluster) {
        // we do not want to test general graphs module here
        return;
      }
      assertTrue(ga.test());
    },

    testRandomWalk: function () {
      if (!isEnterprise && isCluster) {
        // we do not want to test general graphs module here
        return;
      }
      assertTrue(rw.test());
    }
  };
}

jsunity.run(basicTestSuite);

return jsunity.done();
