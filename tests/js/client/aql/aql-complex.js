/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var getRawQueryResults = helper.getRawQueryResults;
const isCov = require("@arangodb/test-helper").versionHas('coverage');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlComplexTestSuite () {
  var numbers = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop("UnitTestsAhuacatlNumbers");
      numbers = internal.db._create("UnitTestsAhuacatlNumbers");

      for (var i = 1; i <= 100; ++i) {
        numbers.save({ "value" : i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop("UnitTestsAhuacatlNumbers");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList1 : function () {
      var expected = [ 1, 2, 3, 129, -4 ];

      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN [ 1, 2, 3, 129, -4 ] FILTER u IN " + JSON.stringify(list) + " RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList2 : function () {
      var expected = [ 1, 2, 3, 129, -4 ];

      var list = [ ];
      for (var i = 1000; i >= -1000; --i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN [ 1, 2, 3, 129, -4 ] FILTER u IN " + JSON.stringify(list) + " RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList3 : function () {
      var expected = [ -4, 1, 2, 3, 129 ];

      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " FILTER u IN [ 1, 2, 3, 129, -4 ] RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList4 : function () {
      var expected = [ 129, 3, 2, 1, -4 ];

      var list = [ ];
      for (var i = 1000; i >= -1000; --i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " FILTER u IN [ 1, 2, 3, 129, -4 ] RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList5 : function () {
      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      }
      var expected = list;
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList6 : function () {
      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      }
      var expected = list;
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " FILTER u IN " + JSON.stringify(list) + " RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test list nesting
////////////////////////////////////////////////////////////////////////////////

    testListNesting1 : function () {
      var list = [ 0 ];
      var last = list;
      let depth = (isCov) ? 50:75;

      for (var i = 1; i < depth; ++i) {
        last.push([ i ]);
        last = last[last.length - 1];
      }

      var expected = [ list ];
      var actual = getQueryResults("RETURN " + JSON.stringify(list));
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long array
////////////////////////////////////////////////////////////////////////////////

    testLongArray1 : function () {
      var vars = { };
      for (var i = 1; i <= 1000; ++i) {
        vars["key" + i] = "value" + i;
      }

      var expected = [ vars ];
      var actual = getRawQueryResults("RETURN " + JSON.stringify(vars));
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test array nesting
////////////////////////////////////////////////////////////////////////////////

    testArrayNesting1 : function () {
      var array = { };
      var last = array;
      let depth = (isCov) ? 30:75;
      for (var i = 1; i < depth; ++i) {
        last["level" + i] = { };
        last = last["level" + i];
      }

      var expected = [ array ];
      var actual = getQueryResults("RETURN " + JSON.stringify(array));
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testBind1 : function () {
      var expected = [ "value9", "value666", "value997", "value999" ];

      var vars = { "res" : [ "value9", "value666", "value997", "value999" ] };
      var list = "[ ";
      for (var i = 1; i <= 1000; ++i) {
        vars["bind" + i] = "value" + i;

        if (i > 1) {
          list += ", ";
        }
        list += "@bind" + i;
      }
      list += " ]";

      var actual = getQueryResults("FOR u IN " + list + " FILTER u IN @res RETURN u", vars);
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlComplexTestSuite);

return jsunity.done();

