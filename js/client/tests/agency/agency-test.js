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
var wait = require("internal").wait;

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

  function readAndCheck(list) {
    var res = readAgency(list);
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
      wait(10);
      assertEqual(readAndCheck([["/x"]]), [{}]);
      writeAndCheck([[{x:12}]]);
      assertEqual(readAndCheck([["/x"]]), [{x:12}]);
      writeAndCheck([[{x:{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/x"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single non-top level key
////////////////////////////////////////////////////////////////////////////////

    testSingleNonTopLevel : function () {
      assertEqual(readAndCheck([["/x/y"]]), [{}]);
      writeAndCheck([[{"x/y":12}]]);
      assertEqual(readAndCheck([["/x/y"]]), [{x:{y:12}}]);
      writeAndCheck([[{"x/y":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/x"]]), [{x:{}}]);
      writeAndCheck([[{"x":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/x"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test preconditions
////////////////////////////////////////////////////////////////////////////////

    testPrecondition : function () {
      writeAndCheck([[{"/a":12}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:12}]);
      writeAndCheck([[{"/a":13},{"/a":12}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:13}]);
      var res = writeAgency([[{"/a":14},{"/a":12}]]); // fail precond {a:12}
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]}); 
      writeAndCheck([[{a:{op:"delete"}}]]);
      // fail precond oldEmpty
      res = writeAgency([[{"a":14},{"a":{"oldEmpty":false}}]]); 
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]}); 
      writeAndCheck([[{"a":14},{"a":{"oldEmpty":true}}]]); // precond oldEmpty
      writeAndCheck([[{"a":14},{"a":{"old":14}}]]);        // precond old
      // fail precond old
      res = writeAgency([[{"a":14},{"a":{"old":13}}]]); 
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]}); 
      writeAndCheck([[{"a":14},{"a":{"isArray":false}}]]); // precond isArray
      // fail precond isArray
      res = writeAgency([[{"a":14},{"a":{"isArray":true}}]]); 
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]});
      // check object precondition
      res = writeAgency([[{"/a/b/c":{"op":"set","new":12}}]]);
      res = writeAgency([[{"/a/b/c":{"op":"set","new":13}},{"a":{"b":{"c":12}}}]]);
      assertEqual(res.statusCode, 200);
      res = writeAgency([[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":12}}}}]]);
      assertEqual(res.statusCode, 412);
      res = writeAgency([[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":13}}}}]]);
      assertEqual(res.statusCode, 200);
      // multiple preconditions
      res = writeAgency([[{"/a":1,"/b":true,"/c":"c"},{"/a":{"oldEmpty":false}}]]);
      assertEqual(readAndCheck([["/a","/b","c"]]), [{a:1,b:true,c:"c"}]);
      res = writeAgency([[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":true}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:1}]);
      res = writeAgency([[{"/a":2},{"/a":{"oldEmpty":true},"/b":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:1}]);
      res = writeAgency([[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":true}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:1}]);
      res = writeAgency([[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/a"]]), [{a:2}]);
      res = writeAgency([[{"/a":3},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/a"]]), [{a:3}]);
      res = writeAgency([[{"/a":2},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:3}]);
      res = writeAgency([[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":true}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:3}]);
      res = writeAgency([[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":false}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/a"]]), [{a:2}]);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test a document
////////////////////////////////////////////////////////////////////////////////

    testDocument : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
    },

    testTransaction : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,4]},"e":12},"d":false}],
                     [{"a":{"b":{"c":[1,2,3]}}}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
    },

    testOpSetNew : function () {
      writeAndCheck([[{"a/z":{"op":"set","new":12}}]]);
      assertEqual(readAndCheck([["a/z"]]), [{"a":{"z":12}}]);
      writeAndCheck([[{"a/y":{"op":"set","new":12, "ttl": 1}}]]);
      assertEqual(readAndCheck([["a/y"]]), [{"a":{"y":12}}]);
      wait(1.250);
      assertEqual(readAndCheck([["a/y"]]), [{a:{}}]);
      writeAndCheck([[{"a/y":{"op":"set","new":12, "ttl": 1}}]]);
      writeAndCheck([[{"a/y":{"op":"set","new":12}}]]);
      assertEqual(readAndCheck([["a/y"]]), [{"a":{"y":12}}]);
      wait(1.250);
      assertEqual(readAndCheck([["a/y"]]), [{"a":{"y":12}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12}}}]]);
      assertEqual(readAndCheck([["/foo/bar/baz"]]), [{"foo":{"bar":{"baz":12}}}]);
      assertEqual(readAndCheck([["/foo/bar"]]), [{"foo":{"bar":{"baz":12}}}]);
      assertEqual(readAndCheck([["/foo"]]), [{"foo":{"bar":{"baz":12}}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12},"ttl":1}}]]);
      wait(1.250);
      assertEqual(readAndCheck([["/foo"]]), [{"foo":{}}]);
      assertEqual(readAndCheck([["/foo/bar"]]), [{"foo":{}}]);
      assertEqual(readAndCheck([["/foo/bar/baz"]]), [{"foo":{}}]);
    },

    testOpPush : function () {
      writeAndCheck([[{"/a/b/c":{"op":"push","new":"max"}}]]);
      assertEqual(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);
      writeAndCheck([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:2.71828182845904523536}}]);          
      writeAndCheck([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);
    },

    testOpRemove : function () {
      writeAndCheck([[{"/a/euler":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/a/euler"]]), [{a:{}}]);
    },
     
    testOpPrepend : function () {
      writeAndCheck([[{"/a/b/c":{"op":"prepend","new":3.141592653589793}}]]);
      assertEqual(readAndCheck([["/a/b/c"]]),
                  [{a:{b:{c:[3.141592653589793,1,2,3,"max"]}}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:2.71828182845904523536}}]);          
      writeAndCheck(
        [[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck(
        [["/a/euler"]]), [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"/a/euler":{"op":"prepend","new":1.25}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[1.25,2.71828182845904523536]}}]);
    },

    testOpShift : function () {
      writeAndCheck([[{"/a/f":{"op":"shift"}}]]); // none before
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/e":{"op":"shift"}}]]); // on empty array
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/b/c":{"op":"shift"}}]]); // on existing array
      assertEqual(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);
      writeAndCheck([[{"/a/b/d":{"op":"shift"}}]]); // on existing scalar
      assertEqual(readAndCheck([["/a/b/d"]]), [{a:{b:{d:[]}}}]);        
    },

    testOpPop : function () {
      writeAndCheck([[{"/a/f":{"op":"pop"}}]]); // none before
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/e":{"op":"pop"}}]]); // on empty array
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/b/c":{"op":"pop"}}]]); // on existing array
      assertEqual(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3]}}}]);        
      writeAndCheck([[{"a/b/d":1}]]); // on existing scalar
      writeAndCheck([[{"/a/b/d":{"op":"pop"}}]]); // on existing scalar
      assertEqual(readAndCheck([["/a/b/d"]]), [{a:{b:{d:[]}}}]);        
    },

    testOpIncrement : function () {
      writeAndCheck([[{"/version":{"op":"delete"}}]]);
      writeAndCheck([[{"/version":{"op":"increment"}}]]); // none before
      assertEqual(readAndCheck([["version"]]), [{version:1}]);
      writeAndCheck([[{"/version":{"op":"increment"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:2}]);
    },

    testOpDecrement : function () {
      writeAndCheck([[{"/version":{"op":"delete"}}]]);
      writeAndCheck([[{"/version":{"op":"decrement"}}]]); // none before
      assertEqual(readAndCheck([["version"]]), [{version:-1}]);
      writeAndCheck([[{"/version":{"op":"decrement"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:-2}]);
    },

    testOpInStrangePlaces : function () {
      writeAndCheck([[{"/op":12}]]);
      assertEqual(readAndCheck([["/op"]]), [{op:12}]);
      writeAndCheck([[{"/op":{op:"delete"}}]]);
      writeAndCheck([[{"/op/a/b/c":{"op":"set","new":{"op":13}}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:13}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"increment"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:14}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"decrement"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:13}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"pop"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[]}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"increment"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:1}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"shift"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[]}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"decrement"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:-1}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"push","new":-1}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[-1]}}}}}]);
      writeAndCheck([[{"/op/a/b/d":{"op":"set","new":{"ttl":14}}}]]);
      assertEqual(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:14}}}}}]);
      writeAndCheck([[{"/op/a/b/d/ttl":{"op":"increment"}}]]);
      assertEqual(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:15}}}}}]);
      writeAndCheck([[{"/op/a/b/d/ttl":{"op":"decrement"}}]]);
      assertEqual(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:14}}}}}]);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(agencyTestSuite);

return jsunity.done();

