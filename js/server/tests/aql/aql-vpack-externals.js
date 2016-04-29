/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for VelocyPack Externals
////////////////////////////////////////////////////////////////////////////////

function aqlVPackExternalsTestSuite () {

  const collName = "UnitTestsVPackExternals";
  const cleanUp = function () {
    db._drop(collName);
  };
  var coll;

  return {

    setUp: function () {
      cleanUp();
      coll = db._create(collName);

      for (var i = 1000; i < 5000; ++i) {
        coll.save({_key: "test" + i});
      }
    },

    tearDown: cleanUp,

    testPlainExternal: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN x`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n._key, "test" + i);
      }
    },

    testExternalInArray: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN [x, x, x]`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n[0]._key, "test" + i);
        assertEqual(n[1]._key, "test" + i);
        assertEqual(n[2]._key, "test" + i);
      }
    },

    testExternalInMixedArray: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN [5, x, x._key]`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n[1]._key, "test" + i);
      }
    },

    testExternalInObject: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN {doc: x}`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n.doc._key, "test" + i);
      }
    },

    testExternalNested: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN [5, {doc: [x]}]`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n[1].doc[0]._key, "test" + i);
      }
    }
  };

}

jsunity.run(aqlVPackExternalsTestSuite);
return jsunity.done();
