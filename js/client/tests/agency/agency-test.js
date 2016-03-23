/*jshint globalstrict:false, strict:true */
/*global assertEqual, ARGUMENTS */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for client-specific functionality
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
/// @author Max Neunhoeffer
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function agencyTestSuite () {
  'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief the agency servers
////////////////////////////////////////////////////////////////////////////////

  var agencyServers = ARGUMENTS;
  var whoseTurn = 0;    // used to do round robin on agencyServers

  var request = require("@arangodb/request");

  function readAgency(list) {
    // We simply try all agency servers in turn until one gives us an HTTP
    // response:
    var res = request({url: agencyServers[whoseTurn] + "/_api/agency/read", method: "POST",
                       followRedirects: true, body: JSON.stringify(list),
                       headers: {"Content-Type": "application/json"}});
    res.bodyParsed = JSON.parse(res.body);
    return res;
  }

  function writeAgency(list) {
    // We simply try all agency servers in turn until one gives us an HTTP
    // response:
    var res = request({url: agencyServers[whoseTurn] + "/_api/agency/write", method: "POST",
                       followRedirects: true, body: JSON.stringify(list),
                       headers: {"Content-Type": "application/json"}});
    res.bodyParsed = JSON.parse(res.body);
    return res;
  }

  function writeAgencyRaw(list) {
      var res = request({url: agencyServers[whoseTurn] + "/_api/agency/write", method: "POST",
                         followRedirects: true, body: list,
                         headers: {"Content-Type": "application/json"}});
      res.bodyParsed = JSON.parse(res.body);
      return res;
  }

  function readAndCheck(list) {
      var res = readAgency(list);
      require ("internal").print(list,res);
    assertEqual(res.statusCode, 200);
    return res.bodyParsed;
  }

  function writeAndCheck(list) {
    var res = writeAgency(list);
    assertEqual(res.statusCode, 200);
  }

  return {


////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single top level key
////////////////////////////////////////////////////////////////////////////////

    testSingleTopLevel : function () {
      assertEqual(readAndCheck([["x"]]), [{}]);
      writeAndCheck([[{x:12}]]);
      assertEqual(readAndCheck([["x"]]), [{x:12}]);
      writeAndCheck([[{x:{"op":"delete"}}]]);
      assertEqual(readAndCheck([["x"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single non-top level key
////////////////////////////////////////////////////////////////////////////////

    testSingleNonTopLevel : function () {
      assertEqual(readAndCheck([["x/y"]]), [{}]);
      writeAndCheck([[{"x/y":12}]]);
      assertEqual(readAndCheck([["x/y"]]), [{x:{y:12}}]);
      writeAndCheck([[{"x/y":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["x"]]), [{x:{}}]);
      writeAndCheck([[{"x":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["x"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a precondition
////////////////////////////////////////////////////////////////////////////////

    testPrecondition : function () {
      writeAndCheck([[{"a":12}]]);
      assertEqual(readAndCheck([["a"]]), [{a:12}]);
      writeAndCheck([[{"a":13},{"a":12}]]);
      assertEqual(readAndCheck([["a"]]), [{a:13}]);
      var res = writeAgency([[{"a":14},{"a":12}]]);
      assertEqual(res.statusCode, 412);
      //assertEqual(res.bodyParsed, {error:true, successes:[]});
      writeAndCheck([[{a:{op:"delete"}}]]);
    },


    testMultiPart : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      assertEqual(readAndCheck(["a/e",[ "d","a/b"]]), [12,{a:{b:{c:[1,2,3]},d:false}}]);
    },

    testOpSetNew : function () {
      writeAndCheck([[{"a/z":{"op":"set","new":12}}]]);
      assertEqual(readAndCheck([["a/z"]]), [{"a":{"z":12}}]);
    },

    testOpNew : function () {
      writeAndCheck([[{"a/z":{"new":13}}]]);
      assertEqual(readAndCheck([["a/z"]]), [{"a":{"z":13}}]);
      writeAndCheck([[{"a/z":{"new":["hello", "world", 1.06]}}]]);
      assertEqual(readAndCheck([["a/z"]]), [{"a":{"z":["hello", "world", 1.06]}}]);
    },

    testOpPush : function () {
      writeAndCheck([[{"a/b/c":{"op":"push","new":"max"}}]]);
      assertEqual(readAndCheck([["a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);
      writeAndCheck([[{"a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{a:{euler:2.71828182845904523536}}]);          
      writeAndCheck([[{"a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{a:{euler:[2.71828182845904523536]}}]);
    },

    testOpRemove : function () {
      writeAndCheck([[{"a/euler":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{}]);
    },
     
    testOpPrepend : function () {
      writeAndCheck([[{"a/b/c":{"op":"prepend","new":3.141592653589793}}]]);
      assertEqual(readAndCheck([["a/b/c"]]), [{a:{b:{c:[3.141592653589793,1,2,3,"max"]}}}]);
      writeAndCheck([[{"a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{a:{euler:2.71828182845904523536}}]);          
      writeAndCheck([[{"a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"a/euler":{"op":"prepend","new":1.25e-6}}]]);
      assertEqual(readAndCheck([["a/euler"]]), [{a:{euler:[1.25e-6,2.71828182845904523536]}}]);
    },

    testOpShift : function () {
      writeAndCheck([[{"a/f":{"op":"shift"}}]]); // none before
      assertEqual(readAndCheck([["a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"a/e":{"op":"shift"}}]]); // on empty array
      assertEqual(readAndCheck([["a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"a/b/c":{"op":"shift"}}]]); // on existing array
      assertEqual(readAndCheck([["a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);        
      writeAndCheck([[{"a/b/d":{"op":"shift"}}]]); // on existing scalar
      assertEqual(readAndCheck([["a/b/d"]]), [{a:{b:{d:[]}}}]);        
    },

    testOpPop : function () {
      writeAndCheck([[{"a/f":{"op":"pop"}}]]); // none before
      assertEqual(readAndCheck([["a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"a/e":{"op":"pop"}}]]); // on empty array
      assertEqual(readAndCheck([["a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"a/b/c":{"op":"pop"}}]]); // on existing array
      assertEqual(readAndCheck([["a/b/c"]]), [{a:{b:{c:[1,2,3]}}}]);        
      writeAndCheck([[{"a/b/d":{"op":"pop"}}]]); // on existing scalar
      assertEqual(readAndCheck([["a/b/d"]]), [{a:{b:{d:[]}}}]);        
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(agencyTestSuite);

return jsunity.done();

