/*jshint globalstrict:false, strict:false, maxlen: 700 */
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

var jsunity = require("jsunity");
var db = require("internal").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function attributeAccessTestSuite () {
  var values = [
    { name: "sir alfred", age: 60, loves: [ "lettuce", "flowers" ] },
    { person: { name: "gadgetto", age: 50, loves: "gadgets" } },
    { name: "everybody", loves: "sunshine" },
    { name: "judge", loves: [ "order", "policing", "weapons" ] },
    "someone"
  ];

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test direct access
////////////////////////////////////////////////////////////////////////////////

    testDirectAccess : function () {
      var result = db._query("RETURN " + JSON.stringify(values[0]) + ".name").toArray();
      assertEqual([ "sir alfred" ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test direct access
////////////////////////////////////////////////////////////////////////////////

    testDirectAccessBind : function () {
      var result = db._query("RETURN @value.name", { value: values[0] }).toArray();
      assertEqual([ "sir alfred" ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-existing attribute
////////////////////////////////////////////////////////////////////////////////

    testNonExistingAttribute : function () {
      var result = db._query("FOR value IN @values RETURN value.broken", { values: values }).toArray();
      assertEqual([ null, null, null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test partially existing attribute
////////////////////////////////////////////////////////////////////////////////

    testPartiallyExistingAttribute : function () {
      var result = db._query("FOR value IN @values RETURN value.name", { values: values }).toArray();
      assertEqual([ "sir alfred", null, "everybody", "judge", null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test partially existing sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testPartiallyExistingSubAttribute : function () {
      var result = db._query("FOR value IN @values RETURN value.person.name", { values: values }).toArray();
      assertEqual([ null, "gadgetto", null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test function call result attribute
////////////////////////////////////////////////////////////////////////////////

    testFunctionCallResultAttribute : function () {
      var result = db._query("FOR value IN @values RETURN NOOPT(PASSTHRU(value.age))", { values: values }).toArray();
      assertEqual([ 60, null, null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test function call result sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testFunctionCallResultAttribute1 : function () {
      var result = db._query("FOR value IN @values RETURN NOOPT(PASSTHRU(value.person.name))", { values: values }).toArray();
      assertEqual([ null, "gadgetto", null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test function call result sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testFunctionCallResultAttribute2 : function () {
      var result = db._query("FOR value IN @values RETURN NOOPT(PASSTHRU(value).person).name", { values: values }).toArray();
      assertEqual([ null, "gadgetto", null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery result attribute
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResultAttribute : function () {
      // won't work (accessing an attribute of an array)
      var result = db._query("RETURN (FOR value IN @values RETURN value).name", { values: values }).toArray();
      assertEqual([ null ], result);
    }

  };
}



function nestedAttributeAccessTestSuite () {
  var values = [
    {'whichOne' : 1, 'foo bar' : 'searchvalue'},
    {'whichOne' : 2, 'foo.bar' : 'searchvalue'},
    {'whichOne' : 3, 'foo': {'bar': 'searchvalue'}}
  ];
  var cn = "testcol";
  var c;
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      c = db._create(cn);
      var i;
      for (i = 0; i < 3 ; i ++) {
        c.save(values[i]);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test direct access
    ////////////////////////////////////////////////////////////////////////////////

    testNestedDirectAccess : function () {
      var queryRes = [
        { 'xpRes': 3, 'q': "FOR item IN @@cn FILTER item.foo.bar == 'searchvalue' RETURN item.whichOne" },
        { 'xpRes': 2, 'q': "FOR item IN @@cn FILTER item.`foo.bar` == 'searchvalue' RETURN item.whichOne" },
        { 'xpRes': 1, 'q': "FOR item IN @@cn FILTER item.`foo bar` == 'searchvalue' RETURN item.whichOne" }

      ];

      var j;
      for (j = 0; j < queryRes.length; j ++) {
        var result = db._query(queryRes[j].q, {'@cn': cn}).toArray();
        assertEqual(queryRes[j].xpRes, result[0]);
      }
    },

    testNestedDirectAccessIndexed : function () {
      var queryRes = [
        { 'xpRes': 3, 'q': "FOR item IN @@cn FILTER item.foo.bar == 'searchvalue' RETURN item.whichOne" },
        { 'xpRes': 2, 'q': "FOR item IN @@cn FILTER item.`foo.bar` == 'searchvalue' RETURN item.whichOne" },
        { 'xpRes': 1, 'q': "FOR item IN @@cn FILTER item.`foo bar` == 'searchvalue' RETURN item.whichOne" }
      ];
      c.ensureIndex({ type: "hash", fields: [ "foo.bar" ], sparse: true });
      // TODO: not yet implemented:   c.ensureIndex({ type: "hash", fields: [ "`foo.bar`" ], sparse: true });
      c.ensureIndex({ type: "hash", fields: [ "foo bar" ], sparse: true });

      var j;
      for (j = 0; j < queryRes.length; j ++) {
        let query = queryRes[j].q;
        var stmt = db._createStatement({query, bindVars: {'@cn': cn}});
        var plan = stmt.explain();
        // TODO: not yet implemented for #1:
        if (j !== 1) {
          assertEqual(plan.plan.nodes[1].type, 'IndexNode');
        }
        var result = db._query(queryRes[j].q, {'@cn': cn}).toArray();
        assertEqual(queryRes[j].xpRes, result[0]);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(attributeAccessTestSuite);
jsunity.run(nestedAttributeAccessTestSuite);

return jsunity.done();

