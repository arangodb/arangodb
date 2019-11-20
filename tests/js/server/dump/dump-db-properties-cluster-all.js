/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dump/reload
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;

function testProperties(name, minReplicationFactor, replicationFactor, sharding){
  db._useDatabase(name);
  let props = db._properties();
  assertEqual(replicationFactor, props.replicationFactor);
  assertEqual(minReplicationFactor, props.minReplicationFactor);
  assertEqual(sharding, props.sharding);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function dumpTestSuite () {
  'use strict';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._useDatabase("_system");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._useDatabase("_system");
    },


    testDatabaseProperties1 : function () {
      testProperties("UnitTestsDumpProperties1", 2, 2, "");
    },

    testDatabaseProperties2 : function () {
      testProperties("UnitTestsDumpProperties2", 2, 2, "single");
    },
    testDatabaseProperties3 : function () {
      testProperties("UnitTestsDumpProperties3", 2, 3, "");
    },

  };
}

jsunity.run(dumpTestSuite);
return jsunity.done();
