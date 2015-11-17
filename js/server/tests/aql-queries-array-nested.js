/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for array indexes in AQL
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function nestedArrayIndexSuite () {
  var cn = "UnitTestsArray";
  var c;

  var indexUsed = function (query, bindVars) {
    var plan = AQL_EXPLAIN(query, bindVars || {}).plan;
    var nodeTypes = plan.nodes.map(function(node) {
      return node.type;
    });
    return (nodeTypes.indexOf("IndexNode") !== -1);
  };

  return {

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testIndexUsageHashFlat : function () {
      c.ensureIndex({ type: "hash", fields: [ "tags[*]" ] });
      c.insert({ tags: [ "foo", "bar", "baz" ] });
      c.insert({ tags: [ "foobar", "quetzalcoatl", "bark" ] });
      c.insert({ tags: [ "b0rk", "bar", "bark" ] });

      [
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*] RETURN doc", { value: "bar" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags RETURN doc", { value: "bar" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*] RETURN doc", { value: "bark" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags RETURN doc", { value: "bark" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*] RETURN doc", { value: "quetzal" }, 0 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags RETURN doc", { value: "quetzal" }, 0 ]
      ].forEach(function(query) {
        var result = AQL_EXECUTE(query[0], query[1]).json;
        assertEqual(query[2], result.length);
        assertTrue(indexUsed(query[0], query[1]));
      });
    },
    
    testIndexUsageHashNested : function () {
      c.ensureIndex({ type: "hash", fields: [ "tags[*].name" ] });
      c.insert({ tags: [ { name: "foo" }, { name: "bar" }, { name: "baz" } ] });
      c.insert({ tags: [ { name: "foobar" }, { name: "quetzalcoatl" }, { name: "bark" } ] });
      c.insert({ tags: [ { name: "b0rk" }, { name: "bar" }, { name: "bark" } ] });

      var result;

      [
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*].name RETURN doc", { value: "bar" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*].name RETURN doc", { value: "bark" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*].name RETURN doc", { value: "quetzal" }, 0 ]
      ].forEach(function(query) {
        result = AQL_EXECUTE(query[0], query[1]).json;
        assertEqual(query[2], result.length);
        assertTrue(indexUsed(query[0], query[1]));
      });
      
      [
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name RETURN doc", { value: "bark" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name RETURN doc", { value: "quetzal" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name[*] RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name[*] RETURN doc", { value: "bark" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name[*] RETURN doc", { value: "quetzal" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags[*].name IN [ @value ] RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags.name[*] IN [ @value ] RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags[*].name IN NOOPT(PASSTHRU(@value)) RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags.name[*] IN NOOPT(PASSTHRU(@value)) RETURN doc", { value: "bar" } ]
      ].forEach(function(query) {
        result = AQL_EXECUTE(query[0], query[1]).json;
        assertFalse(indexUsed(query[0], query[1]), query[0]);
      });
    },
    
    testIndexUsageSkiplistFlat : function () {
      c.ensureIndex({ type: "skiplist", fields: [ "tags[*]" ] });
      c.insert({ tags: [ "foo", "bar", "baz" ] });
      c.insert({ tags: [ "foobar", "quetzalcoatl", "bark" ] });
      c.insert({ tags: [ "b0rk", "bar", "bark" ] });

      [
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*] RETURN doc", { value: "bar" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags RETURN doc", { value: "bar" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*] RETURN doc", { value: "bark" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags RETURN doc", { value: "bark" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*] RETURN doc", { value: "quetzal" }, 0 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags RETURN doc", { value: "quetzal" }, 0 ]
      ].forEach(function(query) {
        var result = AQL_EXECUTE(query[0], query[1]).json;
        assertEqual(query[2], result.length);
        assertTrue(indexUsed(query[0], query[1]));
      });
    },
    
    testIndexUsageSkiplistNested : function () {
      c.ensureIndex({ type: "skiplist", fields: [ "tags[*].name" ] });
      c.insert({ tags: [ { name: "foo" }, { name: "bar" }, { name: "baz" } ] });
      c.insert({ tags: [ { name: "foobar" }, { name: "quetzalcoatl" }, { name: "bark" } ] });
      c.insert({ tags: [ { name: "b0rk" }, { name: "bar" }, { name: "bark" } ] });

      var result;

      [
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*].name RETURN doc", { value: "bar" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*].name RETURN doc", { value: "bark" }, 2 ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags[*].name RETURN doc", { value: "quetzal" }, 0 ]
      ].forEach(function(query) {
        result = AQL_EXECUTE(query[0], query[1]).json;
        assertEqual(query[2], result.length);
        assertTrue(indexUsed(query[0], query[1]));
      });
      
      [
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name RETURN doc", { value: "bark" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name RETURN doc", { value: "quetzal" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name[*] RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name[*] RETURN doc", { value: "bark" } ],
        [ "FOR doc IN " + cn + " FILTER @value IN doc.tags.name[*] RETURN doc", { value: "quetzal" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags[*].name IN @value RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags.name[*] IN @value RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags[*].name IN [ @value ] RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags.name[*] IN [ @value ] RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags[*].name IN NOOPT(PASSTHRU(@value)) RETURN doc", { value: "bar" } ],
        [ "FOR doc IN " + cn + " FILTER doc.tags.name[*] IN NOOPT(PASSTHRU(@value)) RETURN doc", { value: "bar" } ]
      ].forEach(function(query) {
        result = AQL_EXECUTE(query[0], query[1]).json;
        assertFalse(indexUsed(query[0], query[1]), query[0]);
      });
    },

    testNestedSubAttribute : function () {
      c.insert({ tags: [ { name: "foo" }, { name: "bar" }, { name: "baz" } ] });
      c.insert({ tags: [ { name: "quux" } ] });
      c.insert({ tags: [ "foo", "bar", "baz" ] });
      c.insert({ tags: [ "foobar" ] });
      c.insert({ tags: [ { name: "foobar" } ] });
      c.insert({ tags: [ { name: "baz" }, { name: "bark" } ] });
      c.insert({ tags: [ { name: "baz" }, "bark" ] });
      c.insert({ tags: [ { name: "q0rk" }, { name: "foo" } ] });

      var query = "FOR doc IN " + cn + " FILTER @value IN doc.tags[*].name RETURN doc";
      
      var tests = [ 
        [ "foo", 2 ], 
        [ "bar", 1 ], 
        [ "baz", 3 ], 
        [ "quux", 1 ], 
        [ "foobar", 1 ], 
        [ "bark", 1 ], 
        [ "q0rk", 1 ], 
        [ "sn0rk", 0 ],
        [ "Quetzalcoatl", 0 ] 
      ];

      tests.forEach(function(value) {
        var result = AQL_EXECUTE(query, { value: value[0] }).json;
        assertEqual(value[1], result.length);
        assertFalse(indexUsed(query, { value: value[0] }));
      });

      // now try again with an the index
      c.ensureIndex({ type: "hash", fields: [ "tags[*].name" ] });

      tests.forEach(function(value) {
        var result = AQL_EXECUTE(query, { value: value[0] }).json;
        assertEqual(value[1], result.length);
        assertTrue(indexUsed(query, { value: value[0] }));
      });
    },
    
    testNestedAnotherSubAttribute : function () {
      c.insert({ persons: [ { name: { first: "Jane", last: "Doe" }, gender: "f" }, { name: { first: "Joe", last: "Public" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Jack", last: "White" }, gender: "m" }, { name: { first: "John", last: "Black" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Janet", last: "Smith" }, gender: "f" } ] });
      c.insert({ persons: [ { name: { first: "Jill", last: "Jones" }, gender: "f" }, { name: { first: "Jeff", last: "Jackson" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Jimbo", last: "Jonas" }, gender: "m" }, { name: { first: "Jay", last: "Jameson" } } ] });
      c.insert({ persons: [ { name: { first: "Jack", last: "Rabbit" }, gender: "m" }, { name: { first: "Jane", last: "Jackson" }, gender: "f" } ] });
      c.insert({ persons: [ { name: { first: "Janet", last: "Black" }, gender: "f" }, { name: { first: "James", last: "Smith" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Jill", last: "Doe" }, gender: "f" }, { name: { first: "Jeff", last: "Jones" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Janet", last: "Doe" }, gender: "f" }, { name: { first: "Julia", last: "Jones" }, gender: "f" } ] });
      c.insert({ persons: [ { name: { first: "Jo" } }, { name: { first: "Ji" } } ] });

      var query = "FOR doc IN " + cn + " FILTER @value IN doc.persons[*].gender RETURN doc";
      
      var tests = [ 
        [ "m", 7 ], 
        [ "f", 7 ], 
        [ "", 0 ],
        [ null, 2 ],
        [ "Quetzalcoatl", 0 ]
      ];

      tests.forEach(function(value) {
        var result = AQL_EXECUTE(query, { value: value[0] }).json;
        assertEqual(value[1], result.length, value);
        assertFalse(indexUsed(query, { value: value[0] }));
      });

      // now try again with an index
      c.ensureIndex({ type: "hash", fields: [ "persons[*].gender" ] });

      tests.forEach(function(value) {
        var result = AQL_EXECUTE(query, { value: value[0] }).json;
        assertEqual(value[1], result.length, value);
        assertTrue(indexUsed(query, { value: value[0] }));
      });
    },
    
    testNestedSubSubAttribute : function () {
      c.insert({ persons: [ { name: { first: "Jane", last: "Doe" }, gender: "f" }, { name: { first: "Joe", last: "Public" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Jack", last: "White" }, gender: "m" }, { name: { first: "John", last: "Black" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Janet", last: "Smith" }, gender: "f" } ] });
      c.insert({ persons: [ { name: { first: "Jill", last: "Jones" }, gender: "f" }, { name: { first: "Jeff", last: "Jackson" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Jimbo", last: "Jonas" }, gender: "m" }, { name: { first: "Jay", last: "Jameson" } } ] });
      c.insert({ persons: [ { name: { first: "Jack", last: "Rabbit" }, gender: "m" }, { name: { first: "Jane", last: "Jackson" }, gender: "f" } ] });
      c.insert({ persons: [ { name: { first: "Janet", last: "Black" }, gender: "f" }, { name: { first: "James", last: "Smith" }, gender: "m" } ] });
      c.insert({ persons: [ { name: { first: "Jill", last: "Doe" }, gender: "f" }, { name: { first: "Jeff", last: "Jones" }, gender: "m" } ] });

      var query = "FOR doc IN " + cn + " FILTER @value IN doc.persons[*].name.first RETURN doc";
      
      var tests = [ 
        [ "Jane", 2 ], 
        [ "Joe", 1 ], 
        [ "Jack", 2 ], 
        [ "John", 1 ], 
        [ "Janet", 2 ], 
        [ "Jill", 2 ], 
        [ "Jeff", 2 ], 
        [ "Jimbo", 1 ], 
        [ "Jay", 1 ], 
        [ "James", 1 ],
        [ "Quetzalcoatl", 0 ] 
      ];

      tests.forEach(function(value) {
        var result = AQL_EXECUTE(query, { value: value[0] }).json;
        assertEqual(value[1], result.length);
        assertFalse(indexUsed(query, { value: value[0] }));
      });

      // now try again with an index
      c.ensureIndex({ type: "hash", fields: [ "persons[*].name.first" ] });

      tests.forEach(function(value) {
        var result = AQL_EXECUTE(query, { value: value[0] }).json;
        assertEqual(value[1], result.length);
        assertTrue(indexUsed(query, { value: value[0] }));
      });
    }
 
  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(nestedArrayIndexSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
