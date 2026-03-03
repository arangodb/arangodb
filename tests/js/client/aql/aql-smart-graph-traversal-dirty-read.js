/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Lars Maier
/// @author Copyright 2024, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const { deriveTestSuite } = require('@arangodb/test-helper');
const isEnterprise = require("internal").isEnterprise();
const internal = require('internal');

const vn = "UnitTestsVertex";
const en = "UnitTestsEdges";
const gn = "UnitTestsGraph";
const smartGraphAttribute = 'smart';

function BaseTestConfig() {
  return {
    testTraversalWithAllowDirtyReadQueryOption : function () {
      // Insert test data
      for (let i = 0; i < 10; i++) {
        db[vn].insert({ _key: `${i}:v${i}`, value: i, [smartGraphAttribute]: `${i}` });
      }

      // Create edges
      for (let i = 0; i < 9; i++) {
        db[en].insert({
          _from: `${vn}/${i}:v${i}`,
          _to: `${vn}/${i + 1}:v${i + 1}`,
          [smartGraphAttribute]: `${i}`
        });
      }

      // Test traversal with allowDirtyRead in query options
      let result = db._query(
        `FOR v, e, p IN 1..5 OUTBOUND @start GRAPH @graph
         RETURN v`,
        {
          start: `${vn}/0:v0`,
          graph: gn
        },
        {
          allowDirtyReads: true
        }
      ).toArray();

      // Should find vertices 1 through 5
      assertEqual(result.length, 5);
      assertEqual(result[0]._key, "1:v1");
      assertEqual(result[4]._key, "5:v5");

      // Test with path output
      result = db._query(
        `FOR v, e, p IN 1..3 OUTBOUND @start GRAPH @graph
         RETURN p`,
        {
          start: `${vn}/0:v0`,
          graph: gn
        },
        {
          allowDirtyReads: true
        }
      ).toArray();

      assertEqual(result.length, 3);
      assertTrue(result[0].hasOwnProperty('vertices'));
      assertTrue(result[0].hasOwnProperty('edges'));
      assertEqual(result[0].vertices.length, 2); // start + 1 hop
      assertEqual(result[2].vertices.length, 4); // start + 3 hops
    },

    testTraversalWithAllowDirtyReadAndFilters : function () {
      // Insert test data
      for (let i = 0; i < 10; i++) {
        db[vn].insert({ _key: `${i}:v${i}`, value: i, [smartGraphAttribute]: `${i}` });
      }

      // Create edges
      for (let i = 0; i < 9; i++) {
        db[en].insert({
          _from: `${vn}/${i}:v${i}`,
          _to: `${vn}/${i + 1}:v${i + 1}`,
          weight: i + 1,
          [smartGraphAttribute]: `${i}`
        });
      }

      // Test traversal with allowDirtyRead and filters
      let result = db._query(
        `FOR v, e, p IN 1..5 OUTBOUND @start GRAPH @graph
         FILTER v.value > 2
         RETURN v.value`,
        {
          start: `${vn}/0:v0`,
          graph: gn
        },
        {
          allowDirtyReads: true
        }
      ).toArray();

      // Should find vertices with value > 2 (i.e., 3, 4, 5)
      assertEqual(result.length, 3);
      assertEqual(result[0], 3);
      assertEqual(result[1], 4);
      assertEqual(result[2], 5);
    },

    testShortestPathWithAllowDirtyRead : function () {
      // Insert test data - create a simple diamond graph
      db[vn].insert({ _key: "0:start", [smartGraphAttribute]: "0" });
      db[vn].insert({ _key: "1:a", [smartGraphAttribute]: "1" });
      db[vn].insert({ _key: "1:b", [smartGraphAttribute]: "1" });
      db[vn].insert({ _key: "2:end", [smartGraphAttribute]: "2" });

      // Create edges forming a diamond
      db[en].insert({ _from: `${vn}/0:start`, _to: `${vn}/1:a`, [smartGraphAttribute]: "0" });
      db[en].insert({ _from: `${vn}/0:start`, _to: `${vn}/1:b`, [smartGraphAttribute]: "0" });
      db[en].insert({ _from: `${vn}/1:a`, _to: `${vn}/2:end`, [smartGraphAttribute]: "1" });
      db[en].insert({ _from: `${vn}/1:b`, _to: `${vn}/2:end`, [smartGraphAttribute]: "1" });

      // Test shortest path with allowDirtyRead
      let result = db._query(
        `FOR v, e IN OUTBOUND SHORTEST_PATH @start TO @end GRAPH @graph
         RETURN v._key`,
        {
          start: `${vn}/0:start`,
          end: `${vn}/2:end`,
          graph: gn
        },
        {
          allowDirtyReads: true
        }
      ).toArray();

      // Should find a path of length 3 (start, intermediate, end)
      assertEqual(result.length, 3);
      assertEqual(result[0], "0:start");
      assertEqual(result[2], "2:end");
      assertTrue(result[1] === "1:a" || result[1] === "1:b");
    },

    testBidirectionalTraversalWithAllowDirtyRead : function () {
      // Insert test data
      db[vn].insert({ _key: "0:center", [smartGraphAttribute]: "0" });
      db[vn].insert({ _key: "1:out1", [smartGraphAttribute]: "1" });
      db[vn].insert({ _key: "2:out2", [smartGraphAttribute]: "2" });
      db[vn].insert({ _key: "3:in1", [smartGraphAttribute]: "3" });
      db[vn].insert({ _key: "4:in2", [smartGraphAttribute]: "4" });

      // Create outbound edges from center
      db[en].insert({ _from: `${vn}/0:center`, _to: `${vn}/1:out1`, [smartGraphAttribute]: "0" });
      db[en].insert({ _from: `${vn}/0:center`, _to: `${vn}/2:out2`, [smartGraphAttribute]: "0" });

      // Create inbound edges to center
      db[en].insert({ _from: `${vn}/3:in1`, _to: `${vn}/0:center`, [smartGraphAttribute]: "3" });
      db[en].insert({ _from: `${vn}/4:in2`, _to: `${vn}/0:center`, [smartGraphAttribute]: "4" });

      // Test ANY direction traversal with allowDirtyRead
      let result = db._query(
        `FOR v IN 1..1 ANY @start GRAPH @graph
         SORT v._key
         RETURN v._key`,
        {
          start: `${vn}/0:center`,
          graph: gn
        },
        {
          allowDirtyReads: true
        }
      ).toArray();

      // Should find all 4 connected vertices
      assertEqual(result.length, 4);
      assertEqual(result[0], "1:out1");
      assertEqual(result[1], "2:out2");
      assertEqual(result[2], "3:in1");
      assertEqual(result[3], "4:in2");
    }
  };
}

function SmartGraphTraversalDirtyReadGeneralGraph() {
  'use strict';

  const graphs = require("@arangodb/general-graph");

  let suite = {
    setUp: function () {
      graphs._create(gn, [graphs._relation(en, vn, vn)], [], { numberOfShards: 4 });
    },

    tearDown : function () {
      try {
        graphs._drop(gn, true);
      } catch (err) {}

      db._drop(vn);
      db._drop(en);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_GeneralGraph');
  return suite;
}

function SmartGraphTraversalDirtyReadSmartGraph() {
  'use strict';

  const graphs = require("@arangodb/smart-graph");

  let suite = {
    setUp: function () {
      graphs._create(gn, [graphs._relation(en, vn, vn)], [], {
        numberOfShards: 4,
        replicationFactor: 2,
        smartGraphAttribute
      });
    },

    tearDown : function () {
      try {
        graphs._drop(gn, true);
      } catch (err) {}

      db._drop(vn);
      db._drop(en);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_SmartGraph');
  return suite;
}

jsunity.run(SmartGraphTraversalDirtyReadGeneralGraph);

if (isEnterprise) {
  jsunity.run(SmartGraphTraversalDirtyReadSmartGraph);
}

return jsunity.done();
