/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for iresearch usage
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function iResearchAqlTestSuite () {
  var c;
  var v;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      db._dropView("UnitTestsView");
      v = db._createView("UnitTestsView", "iresearch", {});
      var meta = { links: { "UnitTestsCollection": { includeAllFields: true } } };
      v.properties(meta);

      c.save({ a: "foo", b: "bar" }, { waitForSync: true });
      c.save({ a: "foo", b: "baz" }, { waitForSync: true });
      c.save({ a: "bar", b: "foo" }, { waitForSync: true });
      c.save({ a: "baz", b: "foo" }, { waitForSync: true });
    },

    tearDown : function () {
      var meta = { links : { "UnitTestsCollection": null } };
      v.properties(meta);
      v.drop();
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test no fullcount
////////////////////////////////////////////////////////////////////////////////

    testAttributeEqualityFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.a == 'foo' RETURN doc", null, { }).json;

      assertEqual(result.length, 2);
      assertEqual(result[0].a, "foo");
      assertEqual(result[1].a, "foo");
    }



  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(iResearchAqlTestSuite);

return jsunity.done();
