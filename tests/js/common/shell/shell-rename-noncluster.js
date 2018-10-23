/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertMatch */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the random document selector 
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

var arangodb = require("@arangodb");
var db = arangodb.db;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function RenameSuite () {
  'use strict';
  var cn1 = "UnitTestsRename1";
  var cn2 = "UnitTestsRename2";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn2);
      db._drop(cn1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check AQL query after renaming collection
////////////////////////////////////////////////////////////////////////////////

    testAqlAfterRename : function () {
      var c = db._create(cn1);
      for (var i = 0; i < 10000; ++i) {
        db.UnitTestsRename1.save({ value: i });
      }
      c.rename(cn2);
      db._create(cn1);
      for (i = 0; i < 100; ++i) {
        db.UnitTestsRename1.save({ value: i });
      }

      var result = db._query("FOR i IN " + cn1 + " LIMIT 99, 2 RETURN i").toArray();
      assertEqual(1, result.length);
      assertMatch(/^UnitTestsRename1\//, result[0]._id);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RenameSuite);

return jsunity.done();

