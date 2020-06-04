/*jshint globalstrict:false, strict:false */
/*global require, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the AQL statement results encoding
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
let pathForTesting = require('internal').pathForTesting;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: statement results encoding
////////////////////////////////////////////////////////////////////////////////

function StatementResultEncodingSuite () {
  "use strict";

  var c = null;
  var countries = JSON.parse(require("fs").read(pathForTesting('common/countries.json')));

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._useDatabase("_system");
      db._drop("UnitTestsShellStatement");

      c = db._create("UnitTestsShellStatement"); 
      countries.forEach(function(country) {
        c.insert(country);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsShellStatement");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test query all
////////////////////////////////////////////////////////////////////////////////

    testQueryAll : function () {
      var results = db._query("FOR t IN UnitTestsShellStatement RETURN t").toArray();
      var map = { };
      results.forEach(function(result) {
        delete result._key;
        delete result._rev;
        delete result._id;
        assertTrue(result.hasOwnProperty("name"));
        assertTrue(result.hasOwnProperty("alternateName"));
        map[result.name] = result;
      });

      countries.forEach(function(country) {
        assertEqual(country, map[country.name]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test query all
////////////////////////////////////////////////////////////////////////////////

    testQueryAttribute : function () {
      var results = db._query("FOR t IN UnitTestsShellStatement " + 
        "LET l = (FOR x IN t.alternateName FILTER x.`@language` == 'bn' RETURN x) " + 
        "RETURN { name: t.name, alternateName: l }", {}, {profile: 3}).toArray();
      var map = { };
      results.forEach(function(result) {
        delete result._key;
        delete result._rev;
        delete result._id;
        map[result.name] = result;
        assertTrue(result.hasOwnProperty("name"));
        assertTrue(result.hasOwnProperty("alternateName"));
      });

      countries.forEach(function(country) {
        assertEqual(country.alternateName.filter(function(c) { 
          return c["@language"] === "bn"; 
        }), map[country.name].alternateName);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(StatementResultEncodingSuite);
return jsunity.done();

