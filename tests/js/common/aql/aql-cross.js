/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, cross-collection queries
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

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for cross-collection queries
////////////////////////////////////////////////////////////////////////////////

function ahuacatlCrossCollection () {
  var vn1 = "UnitTestsAhuacatlVertex1";
  var vn2 = "UnitTestsAhuacatlVertex2";
  var en  = "UnitTestsAhuacatlEdge";

  var vertex1 = null;
  var vertex2 = null;
  var edge    = null;


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(vn1);
      db._drop(vn2);
      db._drop(en);

      vertex1 = db._create(vn1);
      vertex2 = db._create(vn2);
      edge = db._createEdgeCollection(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vn1);
      db._drop(vn2);
      db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks document function, regular
////////////////////////////////////////////////////////////////////////////////

    testDocument1 : function () {
      var d1 = vertex1.save({ _key: "test1" });

      var actual = getQueryResults("FOR d IN [ DOCUMENT(" + vn1 + ", " + JSON.stringify(d1._id) + ") ] RETURN d");
      assertEqual(1, actual.length);
      
      actual = getQueryResults("FOR d IN DOCUMENT(" + vn1 + ", [ " + JSON.stringify(d1._id) + " ]) RETURN d");
      assertEqual(1, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks document function, cross-collection
////////////////////////////////////////////////////////////////////////////////

    testDocument2 : function () {
      var d2 = vertex2.save({ _key: "test2" });

      var actual = getQueryResults("FOR d IN [ DOCUMENT(" + vn1 + ", " + JSON.stringify(d2._id) + ") ] RETURN d");
      assertEqual(1, actual.length);
      assertEqual(null, actual[0]);
      
      actual = getQueryResults("FOR d IN DOCUMENT(" + vn1 + ", [ " + JSON.stringify(d2._id) + " ]) RETURN d");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks document function
////////////////////////////////////////////////////////////////////////////////

    testDocument3 : function () {
      var d1 = vertex1.save({ _key: "test1" });

      var list = [ vn1 + "/nonexisting", vn1 + "/foxx", d1._id ];

      var actual = getQueryResults("FOR d IN DOCUMENT(" + vn1 + ", " + JSON.stringify(list) + ") RETURN d");
      assertEqual(1, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks document function, correct & cross-collection
////////////////////////////////////////////////////////////////////////////////

    testDocument4 : function () {
      var d1 = vertex1.save({ _key: "test1" });
      var d2 = vertex2.save({ _key: "test2" });

      var list = [ d1._id, d2._id, vn1 + "/nonexisting" ];

      var actual = getQueryResults("FOR d IN DOCUMENT(" + vn1 + ", " + JSON.stringify(list) + ") RETURN d");
      assertEqual(1, actual.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlCrossCollection);

return jsunity.done();

