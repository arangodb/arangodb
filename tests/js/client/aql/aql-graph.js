/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertFalse, assertTrue, assertNotNull, AQL_EXPLAIN, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, graph functions
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;
const helper = require("@arangodb/aql-helper");
const isCluster = require("internal").isCluster();
const gm = require("@arangodb/general-graph");
const getQueryResults = helper.getQueryResults;
const getRawQueryResults = helper.getRawQueryResults;
const assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for graph features
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryEdgesTestSuite() {
  const vn = "UnitTestsAhuacatlVertex";
  let vertex = null;
  let edge = null;

  return {

    setUpAll: function () {
      db._drop(vn);
      db._drop("UnitTestsAhuacatlEdge");

      vertex = db._create(vn, {numberOfShards: 4});
      edge = db._createEdgeCollection("UnitTestsAhuacatlEdge", {numberOfShards: 4});

      vertex.save({_key: "v1", name: "v1"});
      vertex.save({_key: "v2", name: "v2"});
      vertex.save({_key: "v3", name: "v3"});
      vertex.save({_key: "v4", name: "v4"});
      vertex.save({_key: "v5", name: "v5"});
      vertex.save({_key: "v6", name: "v6"});
      vertex.save({_key: "v7", name: "v7"});

      function makeEdge(from, to) {
        edge.save(vn + "/" + from, vn + "/" + to, {_key: from + "" + to, what: from + "->" + to});
      }

      makeEdge("v1", "v2");
      makeEdge("v1", "v3");
      makeEdge("v2", "v3");
      makeEdge("v3", "v4");
      makeEdge("v3", "v6");
      makeEdge("v3", "v7");
      makeEdge("v4", "v2");
      makeEdge("v7", "v3");
      makeEdge("v6", "v3");
    },

    tearDownAll: function () {
      db._drop("UnitTestsAhuacatlVertex");
      db._drop("UnitTestsAhuacatlEdge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges ANY
////////////////////////////////////////////////////////////////////////////////

    testEdgesAny: function () {
      var q = `WITH ${vn} FOR v, e IN ANY @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;

      var actual;
      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v1"});
      assertEqual(actual, ["v1->v2", "v1->v3"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v2"});
      assertEqual(actual, ["v1->v2", "v2->v3", "v4->v2"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3"});
      assertEqual(actual, ["v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v8"});
      assertEqual(actual, []);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/thefox"});
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          const queryStartModified = `WITH ${vn}, thefox FOR v, e IN ANY @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;
          getQueryResults(queryStartModified, {start: "thefox/thefox"});
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        actual = getQueryResults(q, {start: "thefox/thefox"});
        assertEqual(actual, []);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges INBOUND
////////////////////////////////////////////////////////////////////////////////

    testEdgesIn: function () {
      var q = `WITH ${vn} FOR v, e IN INBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;

      var actual;
      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v1"});
      assertEqual(actual, []);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v2"});
      assertEqual(actual, ["v1->v2", "v4->v2"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3"});
      assertEqual(actual, ["v1->v3", "v2->v3", "v6->v3", "v7->v3"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v8"});
      assertEqual(actual, []);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/thefox"});
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          let qModified = `WITH ${vn}, thefox FOR v, e IN INBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;
          getQueryResults(qModified, {start: "thefox/thefox"});
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        actual = getQueryResults(q, {start: "thefox/thefox"});
        assertEqual(actual, []);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges OUTBOUND
////////////////////////////////////////////////////////////////////////////////

    testEdgesOut: function () {
      var q = `WITH ${vn} FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;
      var actual;
      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v1"});
      assertEqual(actual, ["v1->v2", "v1->v3"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v2"});
      assertEqual(actual, ["v2->v3"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v3"});
      assertEqual(actual, ["v3->v4", "v3->v6", "v3->v7"]);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/v8"});
      assertEqual(actual, []);

      actual = getQueryResults(q, {start: "UnitTestsAhuacatlVertex/thefox"});
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          let qModified = `WITH ${vn}, thefox FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;
          getQueryResults(qModified, {start: "thefox/thefox"});
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        actual = getQueryResults(q, {start: "thefox/thefox"});
        assertEqual(actual, []);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesAnyInclVertices: function () {
      "use strict";
      let actual;
      var query = `WITH ${vn} FOR v, e IN ANY @start @@col SORT e.what RETURN v._key`;

      let bindVars = {
        "@col": "UnitTestsAhuacatlEdge"
      };

      bindVars.start = "UnitTestsAhuacatlVertex/v1";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v2", "v3"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v2";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v1", "v3", "v4"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v3";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v1", "v2", "v4", "v6", "v7", "v6", "v7"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v8";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/thefox";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          let queryModified = `WITH ${vn}, thefox FOR v, e IN ANY @start @@col SORT e.what RETURN v._key`;
          bindVars.start = "thefox/thefox";
          getQueryResults(queryModified, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        bindVars.start = "thefox/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, []);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesInInclVerticesNonCollectionBind: function () {
      "use strict";
      let actual;
      let query = `WITH ${vn} FOR v, e IN INBOUND @start @col SORT e.what RETURN v._key`;

      let bindVars = {
        "col": "UnitTestsAhuacatlEdge",
      };

      bindVars.start = "UnitTestsAhuacatlVertex/v1";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/v2";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v1", "v4"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v3";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v1", "v2", "v6", "v7"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v8";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/thefox";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          let queryModified = `WITH ${vn}, thefox FOR v, e IN INBOUND @start @col SORT e.what RETURN v._key`;
          bindVars.start = "thefox/thefox";
          getQueryResults(queryModified, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        bindVars.start = "thefox/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, []);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesInInclVertices: function () {
      "use strict";
      let actual;
      let query = `WITH ${vn} FOR v, e IN INBOUND @start @@col SORT e.what RETURN v._key`;

      let bindVars = {
        "@col": "UnitTestsAhuacatlEdge",
      };

      bindVars.start = "UnitTestsAhuacatlVertex/v1";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/v2";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v1", "v4"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v3";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v1", "v2", "v6", "v7"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v8";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/thefox";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);


      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          let query = `WITH ${vn}, thefox FOR v, e IN INBOUND @start @@col SORT e.what RETURN v._key`;
          bindVars.start = "thefox/thefox";
          getQueryResults(query, bindVars);
          assertEqual(actual, []);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        bindVars.start = "thefox/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, []);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesOutInclVerticesNonCollectionBindInvalidValues: function () {
      "use strict";

      if (isCluster) {
        // [GraphRefactor] Note: Related to #GORDO-1360
        [null, false, true, -1, 0, 1, 2030, 45354.2343, [], {}].forEach(function (value) {
          let modifiedQuery = `WITH ${vn}, ${value} FOR v, e IN OUTBOUND @start @col SORT e.what RETURN v._key`;
          let bindVars = {
            start: "UnitTestsAhuacatlVertex/v1",
            col: value
          };
          assertQueryError(errors.ERROR_QUERY_PARSE.code, modifiedQuery, bindVars);
        });

        [["foo"]].forEach(function (value) {
          let modifiedQuery = `WITH ${vn}, ${value} FOR v, e IN OUTBOUND @start @col SORT e.what RETURN v._key`;
          let bindVars = {
            start: "UnitTestsAhuacatlVertex/v1",
            col: value
          };
          assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, modifiedQuery, bindVars);
        });

      } else {
        let query = `WITH ${vn} FOR v, e IN OUTBOUND @start @col SORT e.what RETURN v._key`;

        [null, false, true, -1, 0, 1, 2030, 45354.2343, [], ["foo"], {}].forEach(function (value) {
          let bindVars = {
            start: "UnitTestsAhuacatlVertex/v1",
            col: value
          };

          assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, query, bindVars);
        });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesOutInclVerticesInvalidCollections: function () {
      "use strict";
      let query = `WITH ${vn} FOR v, e IN OUTBOUND @start @col SORT e.what RETURN v._key`;

      [" ", "NonExisting", "foo bar"].forEach(function (value) {
        let bindVars = {
          start: "UnitTestsAhuacatlVertex/v1",
          col: value
        };

        assertQueryError(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, query, bindVars);
      });

      query = `WITH ${vn} FOR v, e IN OUTBOUND @start @@col SORT e.what RETURN v._key`;

      [" ", "NonExisting", "foo bar"].forEach(function (value) {
        let bindVars = {
          start: "UnitTestsAhuacatlVertex/v1",
          "@col": value
        };

        assertQueryError(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, query, bindVars);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesOutInclVerticesNonCollectionBind: function () {
      "use strict";
      let actual;
      let query = `WITH ${vn} FOR v, e IN OUTBOUND @start @col SORT e.what RETURN v._key`;

      let bindVars = {
        "col": "UnitTestsAhuacatlEdge",
      };

      bindVars.start = "UnitTestsAhuacatlVertex/v1";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v2", "v3"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v2";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v3"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v3";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v4", "v6", "v7"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v8";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/v5";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/thefox";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          let queryModified = `WITH ${vn}, thefox FOR v, e IN OUTBOUND @start @col SORT e.what RETURN v._key`;
          bindVars.start = "thefox/thefox";
          getQueryResults(query, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code);
        }
      } else {
        bindVars.start = "thefox/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, []);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges / vertex combination
////////////////////////////////////////////////////////////////////////////////

    testEdgesOutInclVertices: function () {
      "use strict";
      let actual;
      let query = `WITH ${vn} FOR v, e IN OUTBOUND @start @@col SORT e.what RETURN v._key`;

      let bindVars = {
        "@col": "UnitTestsAhuacatlEdge",
      };

      bindVars.start = "UnitTestsAhuacatlVertex/v1";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v2", "v3"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v2";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v3"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v3";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, ["v4", "v6", "v7"]);

      bindVars.start = "UnitTestsAhuacatlVertex/v8";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/v5";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      bindVars.start = "UnitTestsAhuacatlVertex/thefox";
      actual = getQueryResults(query, bindVars);
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          let queryModified = `WITH ${vn}, thefox FOR v, e IN OUTBOUND @start @@col SORT e.what RETURN v._key`;
          bindVars.start = "thefox/thefox";
          actual = getQueryResults(queryModified, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        bindVars.start = "thefox/thefox";
        actual = getQueryResults(query, bindVars);
        assertEqual(actual, []);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges with filter
////////////////////////////////////////////////////////////////////////////////

    testEdgesFilterExample: function () {
      var q;
      var bindVars = {start: "UnitTestsAhuacatlVertex/v3"};
      var actual;
      q = `WITH ${vn} FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v1->v3"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, ["v1->v3"]);

      q = `WITH ${vn} FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v1->v3" OR e.what == "v3->v6"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, ["v1->v3", "v3->v6"]);

      q = `WITH ${vn} FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, ["v1->v3", "v2->v3", "v3->v4", "v3->v6", "v3->v7", "v6->v3", "v7->v3"]);

      q = `WITH ${vn} FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.non == "matchable"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, []);

      q = `WITH ${vn} FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v1->v3" OR e.non == "matchable"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, ["v1->v3"]);

      q = `WITH ${vn} FOR v, e IN ANY @start UnitTestsAhuacatlEdge 
        FILTER e.what == "v3->v6"
        SORT e.what RETURN e.what`;
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, ["v3->v6"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges when starting with an array
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexArray: function () {
      var q = `WITH ${vn} FOR s IN @start FOR v, e IN OUTBOUND s UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;

      var actual;
      actual = getQueryResults(q, {start: ["UnitTestsAhuacatlVertex/v1", "UnitTestsAhuacatlVertex/v2"]});
      assertEqual(actual, ["v1->v2", "v1->v3", "v2->v3"]);

      actual = getQueryResults(q, {start: [{_id: "UnitTestsAhuacatlVertex/v1"}, {_id: "UnitTestsAhuacatlVertex/v2"}]});
      assertEqual(actual, ["v1->v2", "v1->v3", "v2->v3"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges when starting on an object
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexObject: function () {
      var q = `WITH ${vn} FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;

      var actual;
      actual = getQueryResults(q, {start: {_id: "UnitTestsAhuacatlVertex/v1"}});
      assertEqual(actual, ["v1->v2", "v1->v3"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks edges with illegal start
////////////////////////////////////////////////////////////////////////////////

    testEdgesStartVertexIllegal: function () {
      var q = `WITH ${vn} FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;
      var qArray = `WITH ${vn} FOR s IN @start FOR v, e IN OUTBOUND s UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;
      var actual;

      var bindVars = {start: {_id: "v1"}}; // No collection
      actual = getQueryResults(q, bindVars);
      assertEqual(actual, []);

      if (isCluster) {
        const qModified = `WITH ${vn}, UnitTestTheFuxx FOR v, e IN OUTBOUND @start UnitTestsAhuacatlEdge SORT e.what RETURN e.what`;
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          bindVars = {start: "UnitTestTheFuxx/v1"}; // Non existing collection
          getQueryResults(qModified, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          bindVars = {start: {id: "UnitTestTheFuxx/v1"}};  // No _id attribute
          getQueryResults(qModified, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          bindVars = {start: [{id: "UnitTestTheFuxx/v1"}]}; // Error in Array
          getQueryResults(qModified, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          bindVars = {start: ["UnitTestTheFuxx/v1"]}; // Error in Array
          getQueryResults(qModified, bindVars);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        bindVars = {start: "UnitTestTheFuxx/v1"}; // Non existing collection
        actual = getQueryResults(q, bindVars);
        assertEqual(actual, []);

        bindVars = {start: {id: "UnitTestTheFuxx/v1"}};  // No _id attribute
        actual = getQueryResults(q, bindVars);
        assertEqual(actual, []);

        bindVars = {start: [{id: "UnitTestTheFuxx/v1"}]}; // Error in Array
        actual = getQueryResults(qArray, bindVars);
        // No Error thrown here
        assertEqual(actual, []);

        bindVars = {start: ["UnitTestTheFuxx/v1"]}; // Error in Array
        actual = getQueryResults(qArray, bindVars);
        // No Error thrown here
        assertEqual(actual, []);
      }

      bindVars = {start: ["v1"]}; // Error in Array
      actual = getQueryResults(qArray, bindVars);
      // No Error thrown here
      assertEqual(actual, []);
    }
  };
}

function ahuacatlQueryNeighborsTestSuite() {
  var vertex = null;
  var edge = null;
  var vn = "UnitTestsAhuacatlVertex";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop(vn);
      db._drop("UnitTestsAhuacatlEdge");

      vertex = db._create(vn, {numberOfShards: 4});
      edge = db._createEdgeCollection("UnitTestsAhuacatlEdge", {numberOfShards: 4});

      vertex.save({_key: "v1", name: "v1"});
      vertex.save({_key: "v2", name: "v2"});
      vertex.save({_key: "v3", name: "v3"});
      vertex.save({_key: "v4", name: "v4"});
      vertex.save({_key: "v5", name: "v5"});
      vertex.save({_key: "v6", name: "v6"});
      vertex.save({_key: "v7", name: "v7"});

      function makeEdge(from, to) {
        edge.save(vn + "/" + from, vn + "/" + to, {what: from + "->" + to, _key: from + "_" + to});
      }

      makeEdge("v1", "v2");
      makeEdge("v1", "v3");
      makeEdge("v2", "v3");
      makeEdge("v3", "v4");
      makeEdge("v3", "v6");
      makeEdge("v3", "v7");
      makeEdge("v4", "v2");
      makeEdge("v7", "v3");
      makeEdge("v6", "v3");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._drop(vn);
      db._drop("UnitTestsAhuacatlEdge");
    },

    testNeighborsEdgeFilter: function () {
      // try with bfs/uniqueVertices first
      let query1 = `WITH ${vn} FOR v, e, p IN 0..9 OUTBOUND "${vn}/v1" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} FILTER p.edges[*].what ALL != "v1->v2" && p.edges[*].what ALL != "v1->v3" RETURN v._key`;

      let actual = getQueryResults(query1);
      assertEqual(actual, ["v1"]);

      // now try without bfs/uniqueVertices
      let query2 = `WITH ${vn} FOR v, e, p IN 0..9 OUTBOUND "${vn}/v1" UnitTestsAhuacatlEdge FILTER p.edges[*].what ALL != "v1->v2" && p.edges[*].what ALL != "v1->v3" RETURN v._key`;

      actual = getQueryResults(query2);
      assertEqual(actual, ["v1"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks neighbors
////////////////////////////////////////////////////////////////////////////////

    testNeighborsAny: function () {
      var actual;
      var v1 = "UnitTestsAhuacatlVertex/v1";
      var v2 = "UnitTestsAhuacatlVertex/v2";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v5 = "UnitTestsAhuacatlVertex/v5";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var v8 = "UnitTestsAhuacatlVertex/v8";
      var theFox = "UnitTestsAhuacatlVertex/thefox";
      var queryStart = `WITH ${vn} FOR n IN ANY "`;
      var queryEnd = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n._id RETURN n._id`;
      var queryEndData = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n RETURN n`;

      actual = getQueryResults(queryStart + v1 + queryEnd);
      assertEqual(actual, [v2, v3]);

      actual = getQueryResults(queryStart + v2 + queryEnd);
      assertEqual(actual, [v1, v3, v4]);

      // v6 and v7 are neighbors twice
      actual = getQueryResults(queryStart + v3 + queryEnd);
      assertEqual(actual, [v1, v2, v4, v6, v7]);

      actual = getQueryResults(queryStart + v8 + queryEnd);
      assertEqual(actual, []);

      actual = getQueryResults(queryStart + v5 + queryEnd);
      assertEqual(actual, []);

      actual = getQueryResults(queryStart + theFox + queryEnd);
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          const queryStartModified = `WITH ${vn}, thefox FOR n IN ANY "`;
          getQueryResults(queryStartModified + "thefox/thefox" + queryEnd);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        actual = getQueryResults(queryStart + "thefox/thefox" + queryEnd);
        assertEqual(actual, []);
      }

      // Including Data
      actual = getRawQueryResults(queryStart + v3 + queryEndData);
      actual = actual.map(function (x) {
        assertTrue(x.hasOwnProperty("_key"), "Neighbor has a _key");
        assertTrue(x.hasOwnProperty("_rev"), "Neighbor has a _rev");
        assertTrue(x.hasOwnProperty("_id"), "Neighbor has a _id");
        assertTrue(x.hasOwnProperty("name"), "Neighbor has a custom attribute");
        return x.name;
      });

      assertEqual(actual, ["v1", "v2", "v4", "v6", "v7"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks neighbors inbound
////////////////////////////////////////////////////////////////////////////////

    testNeighborsIn: function () {
      var actual;
      var v1 = "UnitTestsAhuacatlVertex/v1";
      var v2 = "UnitTestsAhuacatlVertex/v2";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v5 = "UnitTestsAhuacatlVertex/v5";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var v8 = "UnitTestsAhuacatlVertex/v8";
      var theFox = "UnitTestsAhuacatlVertex/thefox";

      var queryStart = `WITH ${vn} FOR n IN INBOUND "`;
      var queryEnd = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n._id RETURN n._id`;
      var queryEndData = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n RETURN n`;

      actual = getQueryResults(queryStart + v1 + queryEnd);
      assertEqual(actual, []);

      actual = getQueryResults(queryStart + v2 + queryEnd);
      assertEqual(actual, [v1, v4]);

      actual = getQueryResults(queryStart + v3 + queryEnd);
      assertEqual(actual, [v1, v2, v6, v7]);

      actual = getQueryResults(queryStart + v8 + queryEnd);
      assertEqual(actual, []);

      actual = getQueryResults(queryStart + v5 + queryEnd);
      assertEqual(actual, []);

      actual = getQueryResults(queryStart + theFox + queryEnd);
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          const queryStartModified = `WITH ${vn}, thefox FOR n IN INBOUND "`;
          getQueryResults(queryStartModified + "thefox/thefox" + queryEnd);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        actual = getQueryResults(queryStart + "thefox/thefox" + queryEnd);
        assertEqual(actual, []);
      }

      // Inclunding Data
      actual = getRawQueryResults(queryStart + v3 + queryEndData);
      actual = actual.map(function (x) {
        assertTrue(x.hasOwnProperty("_key"), "Neighbor has a _key");
        assertTrue(x.hasOwnProperty("_rev"), "Neighbor has a _rev");
        assertTrue(x.hasOwnProperty("_id"), "Neighbor has a _id");
        assertTrue(x.hasOwnProperty("name"), "Neighbor has a custom attribute");
        return x.name;
      });
      assertEqual(actual, ["v1", "v2", "v6", "v7"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks outbound neighbors
////////////////////////////////////////////////////////////////////////////////

    testNeighborsOut: function () {
      var actual;
      var v1 = "UnitTestsAhuacatlVertex/v1";
      var v2 = "UnitTestsAhuacatlVertex/v2";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v5 = "UnitTestsAhuacatlVertex/v5";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var v8 = "UnitTestsAhuacatlVertex/v8";
      var theFox = "UnitTestsAhuacatlVertex/thefox";
      var queryStart = `WITH ${vn} FOR n IN OUTBOUND "`;
      var queryEnd = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n._id RETURN n._id`;
      var queryEndData = `" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: "global"} SORT n RETURN n`;

      actual = getQueryResults(queryStart + v1 + queryEnd);
      assertEqual(actual, [v2, v3]);

      actual = getQueryResults(queryStart + v2 + queryEnd);
      assertEqual(actual, [v3]);

      actual = getQueryResults(queryStart + v3 + queryEnd);
      assertEqual(actual, [v4, v6, v7]);

      actual = getQueryResults(queryStart + v8 + queryEnd);
      assertEqual(actual, []);

      actual = getQueryResults(queryStart + v5 + queryEnd);
      assertEqual(actual, []);

      actual = getQueryResults(queryStart + theFox + queryEnd);
      assertEqual(actual, []);

      if (isCluster) {
        try {
          // [GraphRefactor] Note: Related to #GORDO-1360
          const queryStartModified = `WITH ${vn}, thefox FOR n IN OUTBOUND "`;
          getQueryResults(queryStartModified + "thefox/thefox" + queryEnd);
          fail();
        } catch (e) {
          assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        }
      } else {
        actual = getQueryResults(queryStart + "thefox/thefox" + queryEnd);
        assertEqual(actual, []);
      }

      // Inclunding Data
      actual = getRawQueryResults(queryStart + v3 + queryEndData);
      actual = actual.map(function (x) {
        assertTrue(x.hasOwnProperty("_key"), "Neighbor has a _key");
        assertTrue(x.hasOwnProperty("_rev"), "Neighbor has a _rev");
        assertTrue(x.hasOwnProperty("_id"), "Neighbor has a _id");
        assertTrue(x.hasOwnProperty("name"), "Neighbor has a custom attribute");
        return x.name;
      });
      assertEqual(actual, ["v4", "v6", "v7"]);
    },

    testNeighborsEdgeExamples: function () {
      var actual;
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var createQuery = function (start, filter) {
        return `WITH ${vn} FOR n, e IN OUTBOUND "${start}" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: true} ${filter} SORT n._id RETURN n._id`;
      };

      // An empty filter should let all edges through
      actual = getQueryResults(createQuery(v3, ""));

      assertEqual(actual, [v4, v6, v7]);

      // Should be able to handle exactly one object
      actual = getQueryResults(createQuery(v3, 'FILTER e.what == "v3->v4"'));
      assertEqual(actual, [v4]);

      // Should be able to handle a list of objects
      actual = getQueryResults(createQuery(v3, 'FILTER e.what == "v3->v4" OR e.what == "v3->v6"'));
      assertEqual(actual, [v4, v6]);

      // Should be able to handle an id as string
      actual = getQueryResults(createQuery(v3, 'FILTER e._id == "UnitTestsAhuacatlEdge/v3_v6"'));
      assertEqual(actual, [v6]);

      // Should be able to handle a mix of id and objects
      actual = getQueryResults(createQuery(v3, 'FILTER e._id == "UnitTestsAhuacatlEdge/v3_v6" OR e.what == "v3->v4"'));
      assertEqual(actual, [v4, v6]);

      // Should be able to handle internal attributes
      actual = getQueryResults(createQuery(v3, `FILTER e._to == "${v4}"`));
      assertEqual(actual, [v4]);
    },

    testNeighborsWithVertexFilters: function () {
      let actual;
      const v1 = "UnitTestsAhuacatlVertex/v1";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var createQuery = function (start, filter) {
        return `WITH ${vn} FOR n, e, p IN 2 OUTBOUND "${start}" UnitTestsAhuacatlEdge OPTIONS {bfs: true, uniqueVertices: 'global'} ${filter} SORT n._id RETURN n._id`;
      };

      // It should filter on the start vertex
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[0].name == 'v1'"));
      assertEqual(actual, [v4, v6, v7]);

      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[0].name != 'v1'"));
      assertEqual(actual, []);

      // It should filter on the first vertex
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[1].name == 'v3'"));
      assertEqual(actual, [v4, v6, v7]);

      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[1].name != 'v3'"));
      assertEqual(actual, [v3]);

      // It should filter on the second vertex
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[2].name == 'v4'"));
      assertEqual(actual, [v4]);

      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[2].name != 'v4'"));
      assertEqual(actual, [v6, v7]);

      // It should filter on all vertices
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[*].name ALL IN ['v1', 'v3', 'v4', 'v6']"));
      assertEqual(actual, [v4, v6]);
    },

    testNonUniqueBFSWithVertexFilters: function () {
      let actual;
      const v1 = "UnitTestsAhuacatlVertex/v1";
      var v3 = "UnitTestsAhuacatlVertex/v3";
      var v4 = "UnitTestsAhuacatlVertex/v4";
      var v6 = "UnitTestsAhuacatlVertex/v6";
      var v7 = "UnitTestsAhuacatlVertex/v7";
      var createQuery = function (start, filter) {
        return `WITH ${vn} FOR n, e, p IN 2 OUTBOUND "${start}" UnitTestsAhuacatlEdge OPTIONS {bfs: true} ${filter} SORT n._id RETURN n._id`;
      };

      // It should filter on the start vertex
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[0].name == 'v1'"));
      assertEqual(actual, [v3, v4, v6, v7]);

      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[0].name != 'v1'"));
      assertEqual(actual, []);

      // It should filter on the first vertex
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[1].name == 'v3'"));
      assertEqual(actual, [v4, v6, v7]);

      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[1].name != 'v3'"));
      assertEqual(actual, [v3]);

      // It should filter on the second vertex
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[2].name == 'v4'"));
      assertEqual(actual, [v4]);

      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[2].name != 'v4'"));
      assertEqual(actual, [v3, v6, v7]);

      // It should filter on all vertices
      actual = getQueryResults(createQuery(v1, "FILTER p.vertices[*].name ALL IN ['v1', 'v3', 'v4', 'v6']"));
      assertEqual(actual, [v4, v6]);
    }

  };
}

function ahuacatlQueryBreadthFirstTestSuite() {
  let vertex = null;
  let edge = null;
  const vn = "UnitTestsAhuacatlVertex";
  const en = "UnitTestsAhuacatlEdge";
  const center = vn + "/A";

  let cleanUp = function () {
    db._drop(vn);
    db._drop(en);
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
///
///
/// Graph Under Test:
///  +---------+---------+
/// \|/        |        \|/
///  D <- B <- A -> E -> F
///       |   /|\   |
///       |    |    |
///       +--> C <--+
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      cleanUp();

      vertex = db._create(vn, {numberOfShards: 4});
      edge = db._createEdgeCollection(en, {numberOfShards: 4});

      vertex.save({_key: "A"});
      vertex.save({_key: "B"});
      vertex.save({_key: "C"});
      vertex.save({_key: "D"});
      vertex.save({_key: "E"});
      vertex.save({_key: "F"});

      let makeEdge = function (from, to, type) {
        edge.save({
          _from: vn + "/" + from,
          _to: vn + "/" + to,
          _key: from + "" + to,
          type: type
        });
      };

      makeEdge("A", "B", "friend");
      makeEdge("A", "D", "friend");
      makeEdge("A", "E", "enemy");
      makeEdge("A", "F", "enemy");

      makeEdge("B", "C", "enemy");
      makeEdge("B", "D", "friend");

      makeEdge("E", "C", "enemy");
      makeEdge("E", "F", "friend");

      makeEdge("C", "A", "friend");
    },

    tearDownAll: cleanUp,

    testNonUniqueVerticesDefaultDepth: function () {
      var query = `WITH ${vn}
        FOR v IN OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        SORT v._key RETURN v._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual, ["B", "D", "E", "F"]);
    },

    testNonUniqueVerticesMaxDepth2: function () {
      var query = `WITH ${vn}
        FOR v IN 1..2 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        SORT v._key RETURN v._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 8);
      assertEqual(actual, ["B", "C", "C", "D", "D", "E", "F", "F"]);
    },

    testNonUniqueVerticesMinDepth0: function () {
      var query = `WITH ${vn}
        FOR v IN 0..2 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        SORT v._key RETURN v._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 9);
      assertEqual(actual, ["A", "B", "C", "C", "D", "D", "E", "F", "F"]);
    },

    testNonUniqueVerticesMinDepth2: function () {
      var query = `WITH ${vn}
        FOR v IN 2..2 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        SORT v._key RETURN v._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual, ["C", "C", "D", "F"]);
    },

    testUniqueVerticesMaxDepth2: function () {
      var query = `WITH ${vn}
        FOR v IN 1..2 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true, uniqueVertices: 'global'}
        SORT v._key RETURN v._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 5);
      assertEqual(actual, ["B", "C", "D", "E", "F"]);
    },

    testUniqueVerticesMinDepth0: function () {
      var query = `WITH ${vn}
        FOR v IN 0..3 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true, uniqueVertices: 'global'}
        SORT v._key RETURN v._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 6);
      assertEqual(actual, ["A", "B", "C", "D", "E", "F"]);
    },

    testUniqueVerticesMinDepth2: function () {
      var query = `WITH ${vn}
        FOR v IN 2..2 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true, uniqueVertices: 'global'}
        SORT v._key RETURN v._key`;
      var actual;

      // A is directly connected to every other vertex accept "C"
      // So we expect only C to be returned.
      actual = getQueryResults(query);
      assertEqual(actual.length, 1);
      assertEqual(actual, ["C"]);
    },

    testNonUniqueEdgesDefaultDepth: function () {
      var query = `WITH ${vn}
        FOR v,e IN OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        SORT e._key RETURN e._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 4);
      assertEqual(actual, ["AB", "AD", "AE", "AF"]);
    },

    testNonUniqueEdgesMaxDepth2: function () {
      var query = `WITH ${vn}
        FOR v,e IN 1..3 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        SORT e._key RETURN e._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 10);
      assertEqual(actual, ["AB", "AD", "AE", "AF", "BC", "BD", "CA", "CA", "EC", "EF"]);
    },

    testNonUniqueEdgesMinDepth0: function () {
      var query = `WITH ${vn}
        FOR v,e IN 0..3 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: false}
        SORT e._key RETURN e._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 11);
      assertEqual(actual, [null, "AB", "AD", "AE", "AF", "BC", "BD", "CA", "CA", "EC", "EF"]);
    },

    testNonUniqueEdgesMinDepth2: function () {
      var query = `WITH ${vn}
        FOR v,e IN 2..3 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        SORT e._key RETURN e._key`;
      var actual = getQueryResults(query);
      assertEqual(actual.length, 6);
      assertEqual(actual, ["BC", "BD", "CA", "CA", "EC", "EF"]);
    },

    testPathUniqueVertices: function () {
      // Only depth 2 can yield results
      // Depth 3 will return to the startVertex (A)
      // and thereby is non-unique and will be excluded
      const query = `WITH ${vn}
      FOR v, e, p IN 2..4 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true, uniqueVertices: "path"}
        LET keys = CONCAT(p.vertices[*]._key)
        SORT keys RETURN keys`;

      const actual = getQueryResults(query);

      const expected = [
        "AEC",
        "AEF",
        "ABC",
        "ABD"
      ].sort();
      assertEqual(actual.length, expected.length);
      assertEqual(actual, expected);
    },

    testPathUniqueEdges: function () {
      // Only depth 4 and 5 can yield results
      // Depth 6 will have to reuse an edge (A->E or A->B)
      // and thereby is non-unique and will be excluded
      // uniqueEdges: path is the default
      const query = `WITH ${vn}
      FOR v, e, p IN 4..7 OUTBOUND "${center}" ${en}
        OPTIONS {bfs: true}
        LET keys = CONCAT(p.vertices[*]._key)
        SORT keys RETURN keys`;

      const actual = getQueryResults(query);

      const expected = [
        "AECAB",
        "AECAD",
        "AECAF",
        "AECABC",
        "AECABD",
        "ABCAE",
        "ABCAD",
        "ABCAF",
        "ABCAEC",
        "ABCAEF"
      ].sort();
      assertEqual(actual.length, expected.length);
      assertEqual(actual, expected);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for SHORTEST_PATH 
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryShortestPathTestSuite() {
  var vn = "UnitTestsTraversalVertices";
  var en = "UnitTestsTraversalEdges";
  var vertexCollection;
  var edgeCollection;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = db._create(vn, {numberOfShards: 4});
      edgeCollection = db._createEdgeCollection(en, {numberOfShards: 4});

      ["A", "B", "C", "D", "E", "F", "G", "H"].forEach(function (item) {
        vertexCollection.save({_key: item, name: item});
      });

      [["A", "B", 1], ["B", "C", 5], ["C", "D", 1], ["A", "D", 12], ["D", "C", 3], ["C", "B", 2], ["D", "E", 6], ["B", "F", 1], ["E", "G", 5], ["G", "H", 2]].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        var w = item[2];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, {_key: l + r, what: l + "->" + r, weight: w});
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = null;
      edgeCollection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra default config
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraOutbound: function () {
      var query = `WITH ${vn}
                   LET p = (FOR v, e IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} RETURN {v: v._id, e: e._id})
                   LET edges = (FOR e IN p[*].e FILTER e != null RETURN e)
                   LET vertices = p[*].v
                   LET distance = LENGTH(edges)
                   RETURN {edges, vertices, distance}`;
      var actual = getQueryResults(query);

      assertEqual([
        {
          vertices: [
            vn + "/A",
            vn + "/D",
            vn + "/E",
            vn + "/G",
            vn + "/H"
          ],
          edges: [
            en + "/AD",
            en + "/DE",
            en + "/EG",
            en + "/GH"
          ],
          distance: 4
        }
      ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path with a limit, skipping a result
/// Regression test for missing skipSome implementation
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraOutboundSkipFirst: function () {
      const query = `WITH ${vn}
        FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en}
        LIMIT 1,4
        RETURN v._id`;
      const vertices = getQueryResults(query);

      // vertex "A" should have been skipped
      assertEqual([
        vn + "/D",
        vn + "/E",
        vn + "/G",
        vn + "/H"
      ], vertices);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra with includeData: true
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraOutboundIncludeData: function () {
      var query = `WITH ${vn}
                   LET p = (FOR v, e IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} RETURN {v, e})
                   LET edges = (FOR e IN p[*].e FILTER e != null RETURN e)
                   LET vertices = p[*].v
                   LET distance = LENGTH(edges)
                   RETURN {edges, vertices, distance}`;

      var actual = getQueryResults(query);
      assertEqual(actual.length, 1);
      assertTrue(actual[0].hasOwnProperty("vertices"));
      assertTrue(actual[0].hasOwnProperty("edges"));
      var vertices = actual[0].vertices;
      var edges = actual[0].edges;
      assertEqual(vertices.length, edges.length + 1);
      var correct = ["A", "D", "E", "G", "H"];
      for (var i = 0; i < edges.length; ++i) {
        assertEqual(vertices[i]._key, correct[i]);
        assertEqual(vertices[i].name, correct[i]);
        assertEqual(vertices[i + 1].name, correct[i + 1]);
        assertEqual(edges[i]._key, correct[i] + correct[i + 1]);
        assertEqual(edges[i].what, correct[i] + "->" + correct[i + 1]);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path using dijkstra
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraInbound: function () {
      var query = `WITH ${vn} FOR v IN INBOUND SHORTEST_PATH "${vn}/H" TO "${vn}/A" ${en} RETURN v._id`;
      var actual = getQueryResults(query);
      assertEqual([vn + "/H", vn + "/G", vn + "/E", vn + "/D", vn + "/A"], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path with custom distance function
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraDistance: function () {
      var query = `WITH ${vn} FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} OPTIONS {weightAttribute: "weight"} RETURN v._key`;
      var actual = getQueryResults(query);
      assertEqual(["A", "B", "C", "D", "E", "G", "H"], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path, with cycles
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraCycles: function () {
      [["B", "A"], ["C", "A"], ["D", "B"]].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, {_key: l + r, what: l + "->" + r});
      });

      var query = `WITH ${vn} FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/H" ${en} RETURN v._key`;
      var actual = getQueryResults(query);

      assertEqual(["A", "D", "E", "G", "H"], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief shortest path, non-connected vertices
////////////////////////////////////////////////////////////////////////////////

    testShortestPathDijkstraNotConnected: function () {
      // this item is not connected to any other
      vertexCollection.save({_key: "J", name: "J"});

      let query;
      if (isCluster) {
        query = `WITH ${vn} FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/J" ${en} RETURN v._key`;
      } else {
        query = `FOR v IN OUTBOUND SHORTEST_PATH "${vn}/A" TO "${vn}/J" ${en} RETURN v._key`;
      }
      var actual = getQueryResults(query);

      assertEqual([], actual);
    },

    testKPathsConnectedButInnerVertexDeleted: function () {
      // Find the path(s): A -> B -> F (which is valid)
      // Case: B will be deleted before query execution.
      // This is valid, but the query needs to report an error!
      let key = 'B';
      let item = `${vn}/${key}`;
      vertexCollection.remove(item);

      // Execute without fail and check
      let query = `WITH ${vn} FOR path IN 1..3 OUTBOUND K_PATHS "${vn}/A" TO "${vn}/F" ${en} RETURN path.vertices[* RETURN CURRENT._key]`;
      let actual = getQueryResults(query);
      assertEqual(2, actual.length); // two paths because testShortestPathDijkstraCycles test added edges
      assertEqual([['A', 'null', 'F'], ['A', 'D', 'null', 'F']], actual);

      // Now execute with fail on warnings
      try {
        db._query(query, null, {"failOnWarning": true});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      // re-add vertex to let environment stay as is has been before the test
      vertexCollection.save({_key: key, name: key});
    },

    testAllPathsConnectedButInnerVertexDeleted: function () {
      // Find the path(s): A -> B -> F (which is valid)
      // Case: B will be deleted before query execution.
      // This is valid, but the query needs to report an error!
      let key = 'B';
      let item = `${vn}/${key}`;
      vertexCollection.remove(item);

      // Execute without fail and check
      let query = `WITH ${vn} FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${vn}/A" TO "${vn}/F" ${en} RETURN path.vertices[* RETURN CURRENT._key]`;
      let actual = getQueryResults(query);
      assertEqual(1, actual.length);
      assertEqual([['A', 'null', 'F']], actual);

      // Now execute with fail on warnings
      try {
        db._query(query, null, {"failOnWarning": true});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      // re-add vertex to let environment stay as is has been before the test
      vertexCollection.save({_key: key, name: key});
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ShortestPath with intentional failures
////////////////////////////////////////////////////////////////////////////////
function kPathsTestSuite() {
  const gn = "UnitTestGraph";
  const vn = "UnitTestV";
  const en = "UnitTestE";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      gm._create(gn, [gm._relation(en, vn, vn)]);

      ["s", "t", "a", "b", "c", "d", "e", "f", "g"].map((elem) => {
        db[vn].insert({_key: elem});
      });

      [
        ["s", "a"], ["s", "b"], ["a", "b"], ["a", "c"], ["b", "c"],
        ["c", "d"], ["d", "t"],
        ["c", "e"], ["e", "t"],
        ["c", "f"], ["f", "t"],
        ["c", "g"], ["g", "t"]
      ].map(([a, b]) => {
        db[en].insert({_from: `${vn}/${a}`, _to: `${vn}/${b}`});
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      gm._drop(gn, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we are able to find all paths when using ANY direction with KPATHs
/// One of the edges is used in both directions.
////////////////////////////////////////////////////////////////////////////////

    testkPathsAnyUseEdgeTwice: function () {
      let outbound = db._query(`
        FOR p IN 1..10 OUTBOUND K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.vertices[*]._key
      `);

      let any = db._query(`
        FOR p IN 1..10 ANY K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.vertices[*]._key
      `);

      assertEqual(outbound.toArray().length, 12);
      assertEqual(any.toArray().length, 16);
    },
    
    testkPathsWithVertices1: function () {
      const queryOutbound = `
        FOR p IN 1..10 OUTBOUND K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p
      `;
      
      const queryAny = `
        FOR p IN 1..10 ANY K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p
      `;

      let plan = db._createStatement(queryOutbound).explain().plan;
      assertEqual(-1, plan.rules.indexOf("optimize-paths"));
      let nodes = plan.nodes.filter((n) => n.type === 'EnumeratePathsNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].options.hasOwnProperty("produceVertices"));
      assertTrue(nodes[0].options.produceVertices);
      
      plan = db._createStatement(queryAny).explain().plan;
      nodes = plan.nodes.filter((n) => n.type === 'EnumeratePathsNode');
      assertEqual(-1, plan.rules.indexOf("optimize-paths"));
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].options.hasOwnProperty("produceVertices"));
      assertTrue(nodes[0].options.produceVertices);

      let outbound = db._query(queryOutbound);
      assertEqual(outbound.toArray().length, 12);
      outbound.toArray().forEach((doc) => {
        assertTrue(doc.hasOwnProperty("vertices"));
        assertTrue(doc.hasOwnProperty("edges"));
      });

      let any = db._query(queryAny);
      assertEqual(any.toArray().length, 16);
      any.toArray().forEach((doc) => {
        assertTrue(doc.hasOwnProperty("vertices"));
        assertTrue(doc.hasOwnProperty("edges"));
      });
    },
    
    testkPathsWithVertices2: function () {
      const queryOutbound = `
        FOR p IN 1..10 OUTBOUND K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.vertices
      `;
      
      const queryAny = `
        FOR p IN 1..10 ANY K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.vertices
      `;

      let plan = db._createStatement(queryOutbound).explain().plan;
      assertEqual(-1, plan.rules.indexOf("optimize-paths"));
      let nodes = plan.nodes.filter((n) => n.type === 'EnumeratePathsNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].options.hasOwnProperty("produceVertices"));
      assertTrue(nodes[0].options.produceVertices);
      
      plan = db._createStatement(queryAny).explain().plan;
      nodes = plan.nodes.filter((n) => n.type === 'EnumeratePathsNode');
      assertEqual(-1, plan.rules.indexOf("optimize-paths"));
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].options.hasOwnProperty("produceVertices"));
      assertTrue(nodes[0].options.produceVertices);

      let outbound = db._query(queryOutbound);
      assertEqual(outbound.toArray().length, 12);
      outbound.toArray().forEach((doc) => {
        assertNotNull(doc);
      });

      let any = db._query(queryAny);
      assertEqual(any.toArray().length, 16);
      any.toArray().forEach((doc) => {
        assertNotNull(doc);
      });
    },
    
    testkPathsNoVertices: function () {
      const queryOutbound = `
        FOR p IN 1..10 OUTBOUND K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.edges
      `;
      
      const queryAny = `
        FOR p IN 1..10 ANY K_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.edges
      `;

      let plan = db._createStatement(queryOutbound).explain().plan;
      assertNotEqual(-1, plan.rules.indexOf("optimize-paths"));
      let nodes = plan.nodes.filter((n) => n.type === 'EnumeratePathsNode');
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].options.hasOwnProperty("produceVertices"));
      assertFalse(nodes[0].options.produceVertices);
      
      plan = db._createStatement(queryAny).explain().plan;
      nodes = plan.nodes.filter((n) => n.type === 'EnumeratePathsNode');
      assertNotEqual(-1, plan.rules.indexOf("optimize-paths"));
      assertEqual(1, nodes.length);
      assertTrue(nodes[0].options.hasOwnProperty("produceVertices"));
      assertFalse(nodes[0].options.produceVertices);

      let outbound = db._query(queryOutbound);
      assertEqual(outbound.toArray().length, 12);

      let any = db._query(queryAny);
      assertEqual(any.toArray().length, 16);
    },
  };
}

function allShortestPathsTestSuite() {
  const gn = "UnitTestGraph";
  const vn = "UnitTestV";
  const en = "UnitTestE";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      gm._create(gn, [gm._relation(en, vn, vn)]);

      ["s", "t", "a", "b", "c", "d", "e", "f", "g", "x", "y", "z"].map((elem) => {
        db[vn].insert({_key: elem});
      });

      [
        ["s", "a"], ["s", "b"], ["a", "b"], ["a", "c"], ["b", "c"],
        ["c", "d"], ["d", "t"],
        ["c", "e"], ["e", "t"],
        ["c", "f"], ["f", "t"],
        ["c", "g"], ["g", "t"],
        ["s", "x"], ["y", "x"], ["y", "z"], ["z", "y"], ["z", "t"]
      ].map(([a, b]) => {
        db[en].insert({_from: `${vn}/${a}`, _to: `${vn}/${b}`});
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      gm._drop(gn, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we are able to find all shortest paths when using ANY direction with ALL_SHORTEST_PATHS
/// One of the edges is used in both directions.
////////////////////////////////////////////////////////////////////////////////

    testAllShortestPathsAnyUseEdgeTwice: function () {
      let outbound = db._query(`
        FOR p IN OUTBOUND ALL_SHORTEST_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.vertices[*]._key
      `);

      let any = db._query(`
        FOR p IN ANY ALL_SHORTEST_PATHS "${vn}/s" to "${vn}/t"
          GRAPH ${gn}
        RETURN p.vertices[*]._key
      `);

      assertEqual(outbound.toArray().length, 8);
      assertEqual(any.toArray().length, 10); // two new paths: S -> X <- Y <-> Z -> T
    }
  };
}

function ShortestPathErrorTestSuite() {

  const graphName = "UnitTestGraph";
  const vName = "UnitTestVertices";
  const eName = "UnitTestEdges";

  const keyA = "A";
  const keyB = "B";
  const keyC = "C";
  const keyD = "D";
  const keyE = "E";

  function createGraph() {
    /*
     * Graph:
     *   A  -> B  -> E
     *    ⬊ C -> D ⬈
     */
    gm._create(graphName, [gm._relation(eName, vName, vName)], [], {});

    const vertexes = [
      {_key: keyA, value: 1},
      {_key: keyB, value: 1},
      {_key: keyC, value: 1},
      {_key: keyD, value: 1},
      {_key: keyE, value: 1}
    ];

    const edges = [
      {_from: `${vName}/${keyA}`, _to: `${vName}/${keyB}`, weight: 1},
      {_from: `${vName}/${keyB}`, _to: `${vName}/${keyE}`, weight: -1},
      {_from: `${vName}/${keyA}`, _to: `${vName}/${keyC}`, weight: -1},
      {_from: `${vName}/${keyC}`, _to: `${vName}/${keyD}`, weight: 1},
      {_from: `${vName}/${keyD}`, _to: `${vName}/${keyE}`, weight: -1}
    ];

    db[vName].insert(vertexes);
    db[eName].insert(edges);
  }


  return {
    setUpAll: function () {
      createGraph();
    },

    tearDownAll: function () {
      gm._drop(graphName, true);
    },

    testShortestPathNegativeDefaultEdgeWeight: function () {
      const source = `${vName}/${keyA}`;
      const target = `${vName}/${keyD}`;

      const bindVars = {
        weight: -1
      };

      const query = `
        FOR path IN OUTBOUND SHORTEST_PATH "${source}" TO "${target}" GRAPH "${graphName}"
          OPTIONS {defaultWeight: @weight}
          RETURN path
      `;

      try {
        let res = db._query(query, bindVars);
        fail();
      } catch (err) {
        console.warn(err);
        assertEqual(err.errorNum, internal.errors.ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT.code);
      }
    },

    testShortestPathNegativeEdgeWeight: function () {
      const source = `${vName}/${keyA}`;
      const target = `${vName}/${keyD}`;

      const bindVars = {
        weight: 1,
        weightAttributeToUse: "weight"
      };

      const query = `
        FOR path IN OUTBOUND SHORTEST_PATH "${source}" TO "${target}" GRAPH "${graphName}"
          OPTIONS {defaultWeight: @weight, weightAttribute: @weightAttributeToUse}
          RETURN path
      `;

      try {
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, internal.errors.ERROR_GRAPH_NEGATIVE_EDGE_WEIGHT.code);
      }
    },

    testShortestPathInvalidOptionsParameter: function () {
      /* SHORTEST_PATH */
      const source = `${vName}/${keyA}`;
      const target = `${vName}/${keyD}`;

      const query = `
        FOR path IN OUTBOUND SHORTEST_PATH "${source}" TO "${target}" GRAPH "${graphName}"
          OPTIONS {invalidParameter: "IShouldCreateAWarning"}
          RETURN path
      `;

      const result = db._query(query);
      const extra = result.getExtra();
      assertEqual(1, extra.warnings.length);
      assertEqual(extra.warnings[0].code, internal.errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code);
      assertTrue(extra.warnings[0].message.includes('in SHORTEST_PATH statement'));
    },

    testKPathsInvalidOptionsParameter: function () {
      /* K_PATHS */
      const source = `${vName}/${keyA}`;
      const target = `${vName}/${keyD}`;

      const query = `
        FOR path IN OUTBOUND K_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          OPTIONS {invalidParameter: "IShouldCreateAWarning"}
          RETURN path
      `;

      const result = db._query(query);
      const extra = result.getExtra();
      assertEqual(1, extra.warnings.length);
      assertEqual(extra.warnings[0].code, internal.errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code);
      assertTrue(extra.warnings[0].message.includes('in K_PATHS statement'));
    },

    testKShortestPathsInvalidOptionsParameter: function () {
      /* K_SHORTEST_PATHS */
      const source = `${vName}/${keyA}`;
      const target = `${vName}/${keyD}`;

      const query = `
        FOR path IN OUTBOUND K_SHORTEST_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          OPTIONS {invalidParameter: "IShouldCreateAWarning"}
          RETURN path
      `;

      const result = db._query(query);
      const extra = result.getExtra();
      assertEqual(1, extra.warnings.length);
      assertEqual(extra.warnings[0].code, internal.errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code);
      assertTrue(extra.warnings[0].message.includes('in K_SHORTEST_PATHS statement'));
    },

    testAllShortestPathsInvalidOptionsParameter: function () {
      /* ALL_SHORTEST_PATHS */
      const source = `${vName}/${keyA}`;
      const target = `${vName}/${keyD}`;

      const query = `
        FOR path IN OUTBOUND ALL_SHORTEST_PATHS "${source}" TO "${target}" GRAPH "${graphName}"
          OPTIONS {invalidParameter: "IShouldCreateAWarning"}
          RETURN path
      `;

      const result = db._query(query);
      const extra = result.getExtra();
      assertEqual(1, extra.warnings.length);
      assertEqual(extra.warnings[0].code, internal.errors.ERROR_QUERY_INVALID_OPTIONS_ATTRIBUTE.code);
      assertTrue(extra.warnings[0].message.includes('in ALL_SHORTEST_PATHS statement'));
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryEdgesTestSuite);
jsunity.run(ahuacatlQueryNeighborsTestSuite);
jsunity.run(ahuacatlQueryBreadthFirstTestSuite);
jsunity.run(ahuacatlQueryShortestPathTestSuite);
jsunity.run(kPathsTestSuite);
jsunity.run(allShortestPathsTestSuite);
jsunity.run(ShortestPathErrorTestSuite);

return jsunity.done();
