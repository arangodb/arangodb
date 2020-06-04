/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;

const vc1Name = "v1";
const vc2Name = "v2";
const ecName = "edges";

var cleanup = function() {
  db._drop(vc1Name);
  db._drop(vc2Name);
  db._drop(ecName);
};

var createBaseGraph = function () {
  const vc1 = db._create(vc1Name);
  const vc2 = db._create(vc2Name);
  const ec = db._createEdgeCollection(ecName);

  for (let i = 0; i < 10; ++i) {
    const doc = { _key: "node_" + i, value: i };
    vc1.save(doc);
    vc2.save(doc);
  }

  for (let i = 0; i < 10; ++i) {
    ec.save({_from: vc1Name + "/node_" + i, _to: vc2Name + "/node_" + i});
    ec.save({_from: vc2Name + "/node_" + i, _to: vc1Name + "/node_" + i});
    if (i < 9) {
      ec.save({_from: vc1Name + "/node_" + i, _to: vc1Name + "/node_" + (i + 1)});
    }
    if (i > 0) {
      ec.save({_from: vc2Name + "/node_" + i, _to: vc2Name + "/node_" + (i - 1)});
    }
  }
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function vertexCollectionRestrictionSuite() {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      cleanup();
      createBaseGraph();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      cleanup();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test object access for path object
////////////////////////////////////////////////////////////////////////////////

    testNoRestriction : function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName}).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testNoPracticalRestriction: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       OPTIONS {vertexCollections: ["${vc1Name}", "${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName }).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testRestrict1: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       OPTIONS {vertexCollections: ["${vc1Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName }).toArray();
      const expected = [
        vc1Name + "/node_8",
      ];

      assertEqual(actual, expected);
    },

    testRestrict2: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       OPTIONS {vertexCollections: ["${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName }).toArray();
      const expected = [
        vc2Name + "/node_3",
      ];

      assertEqual(actual, expected);
    },

    testNoRestrictionBfs : function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       OPTIONS {bfs: true}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName}).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testNoPracticalRestrictionBfs: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       OPTIONS {bfs: true, vertexCollections: ["${vc1Name}", "${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName }).toArray();
      const expected = [
        vc1Name + "/node_4",
        vc1Name + "/node_6",
        vc1Name + "/node_8",
        vc2Name + "/node_3",
        vc2Name + "/node_5",
        vc2Name + "/node_7",
      ];

      assertEqual(actual, expected);
    },

    testRestrict1Bfs: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       OPTIONS {bfs: true, vertexCollections: ["${vc1Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName }).toArray();
      const expected = [
        vc1Name + "/node_8",
      ];

      assertEqual(actual, expected);
    },

    testRestrict2Bfs: function () {
      const query = `WITH @@vc1, @@vc2
                     FOR v IN 3..3 OUTBOUND "${vc1Name}/node_5" @@ec
                       OPTIONS {bfs: true, vertexCollections: ["${vc2Name}"]}
                       SORT v._id
                       RETURN DISTINCT v._id`;

      const actual = db._query(query, {'@vc1': vc1Name, '@vc2': vc2Name, '@ec': ecName }).toArray();
      const expected = [
        vc2Name + "/node_3",
      ];

      assertEqual(actual, expected);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(vertexCollectionRestrictionSuite);

return jsunity.done();

