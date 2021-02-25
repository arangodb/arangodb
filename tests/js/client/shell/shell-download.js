/*jshint globalstrict:false, strict:false, sub: true */
/*global fail, assertEqual, assertTrue, assertFalse, assertNotEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test download function
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var arangodb = require("@arangodb");
var fs = require("fs");

require("@arangodb/test-helper").waitForFoxxInitialized();

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function DownloadSuite () {
  'use strict';
  var tempDir;
  var tempName;

  var buildUrl = function (append) {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:') + "/_admin/echo" + append;
  };
  
  var buildUrlBroken = function (append) {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:') + "/_not-there" + append;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      // create a name for a temporary directory we will run all tests in
      // as we are creating a new name, this shouldn't collide with any existing
      // directories, and we can also remove our directory when the tests are
      // over
      tempDir = fs.join(fs.getTempFile('', false));

      try {
        // create this directory before any tests
        fs.makeDirectoryRecursive(tempDir);
        tempName = fs.join(tempDir, 'foo');
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // some basic security checks as we don't want to unintentionally remove "." or "/"
      if (tempDir.length > 5) {
        // remove our temporary directory with all its subdirectories
        // we created it, so we don't care what's in it
        fs.removeDirectoryRecursive(tempDir);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET
////////////////////////////////////////////////////////////////////////////////
/*
    testGet : function () {
      var result, response = internal.download(buildUrl(""));

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("GET", result.requestType);
      assertEqual(0, Object.getOwnPropertyNames(result.parameters).length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET
////////////////////////////////////////////////////////////////////////////////

    testGetParams : function () {
      var result, response = internal.download(buildUrl("?foo=1&bar=2&code=201"));

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("GET", result.requestType);
      assertEqual(3, Object.getOwnPropertyNames(result.parameters).length);
      assertEqual("1", result.parameters["foo"]);
      assertEqual("2", result.parameters["bar"]);
      assertEqual("201", result.parameters["code"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET
////////////////////////////////////////////////////////////////////////////////

    testGetSave : function () {
      var result, response = internal.download(buildUrl("?foo=1&bar=baz"), undefined, { }, tempName );

      assertEqual(200, response.code);

      result = JSON.parse(fs.read(tempName));
      assertEqual("GET", result.requestType);
      assertEqual(2, Object.getOwnPropertyNames(result.parameters).length);
      assertEqual("1", result.parameters["foo"]);
      assertEqual("baz", result.parameters["bar"]);
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET error
////////////////////////////////////////////////////////////////////////////////

    testGetErrorNoReturn : function () {
      var response = internal.download(buildUrlBroken("?foo=1&bar=baz&code=404"),
                                       undefined, { timeout: 300 }, tempName );
      assertEqual(404, response.code);

      assertFalse(fs.exists(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET error
////////////////////////////////////////////////////////////////////////////////

    testGetErrorDirect : function () {
      var result, response = internal.download(buildUrlBroken("?foo=1&bar=baz&code=404"),
                                               undefined, { timeout: 300, returnBodyOnError: true });

      assertEqual(404, response.code);
     
      result = JSON.parse(response.body);
      assertTrue(result.error);
      assertEqual(404, result.code);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET error
////////////////////////////////////////////////////////////////////////////////

    testGetErrorSave : function () {
      var result, response = internal.download(buildUrlBroken("?foo=1&bar=baz&code=404"),
                                               undefined, { timeout: 300, returnBodyOnError: true },
                                               tempName);

      assertEqual(404, response.code);
      
      result = JSON.parse(fs.read(tempName));
      assertTrue(result.error);
      assertEqual(404, result.code);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test response headers
////////////////////////////////////////////////////////////////////////////////
/*
    testResponseHeaders : function () {
      var result, response = internal.download(buildUrl(""));

      assertEqual(200, response.code);
      
      result = JSON.parse(response.body);
      assertEqual("GET", result.requestType);
      assertTrue(response.body.length > 0);
      assertEqual(response.headers["content-length"], response.body.length);
      assertNotEqual(response.headers["connection"], "");
      assertEqual("200 OK", response.headers["http/1.1"]);
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test request headers
////////////////////////////////////////////////////////////////////////////////
/*
    testRequestHeaders : function () {
      var result, response = internal.download(buildUrl(""),
                                               "I am broken", 
                                               { method: "post",
                                                 headers: {
                                                   "Content-Type" : "x/broken"
                                                 } });

      assertEqual(200, response.code);
      
      result = JSON.parse(response.body);
      assertEqual("POST", result.requestType);
      assertEqual(0, Object.getOwnPropertyNames(result.parameters).length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http POST
////////////////////////////////////////////////////////////////////////////////

    testPost : function () {
      var result, response = internal.download(buildUrl("?foo=1&bar=2&code=202"),
                                               "meow=1&baz=bam",
                                               { method: "POST",
                                                 headers: {
                                                   "Content-Type" :
                                                   "application/x-www-form-urlencoded" } });

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("POST", result.requestType);
      assertEqual(3, Object.getOwnPropertyNames(result.parameters).length);
      assertEqual("1", result.parameters["foo"]);
      assertEqual("2", result.parameters["bar"]);
      assertEqual("202", result.parameters["code"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http POST
////////////////////////////////////////////////////////////////////////////////

    testPostEmpty1 : function () {
      var result, response = internal.download(buildUrl("?code=200"), "",
                                               { method: "POST",
                                                 headers: { "Content-Type" :
                                                            "application/x-www-form-urlencoded" } });

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("POST", result.requestType);
      assertEqual(1, Object.getOwnPropertyNames(result.parameters).length);
      assertEqual("200", result.parameters["code"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http POST
////////////////////////////////////////////////////////////////////////////////

    testPostEmpty2 : function () {
      var result, response = internal.download(buildUrl("?code=200"),
                                               undefined,
                                               { method: "POST",
                                                 headers: {
                                                   "Content-Type" :
                                                   "application/x-www-form-urlencoded"
                                                 } });

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("POST", result.requestType);
      assertEqual(1, Object.getOwnPropertyNames(result.parameters).length);
      assertEqual("200", result.parameters["code"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http PUT
////////////////////////////////////////////////////////////////////////////////

    testPut : function () {
      var result, response = internal.download(buildUrl(""),
                                               "",
                                               { method: "put",
                                                 headers: {
                                                   "Content-Type" :
                                                   "application/x-www-form-urlencoded"
                                                 } });

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("PUT", result.requestType);
      assertEqual(0, Object.getOwnPropertyNames(result.parameters).length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http PATCH
////////////////////////////////////////////////////////////////////////////////

    testPatch : function () {
      var result, response = internal.download(buildUrl(""), "",
                                               { method: "patch",
                                                 headers: {
                                                   "Content-Type" :
                                                   "application/x-www-form-urlencoded"
                                                 } });

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("PATCH", result.requestType);
      assertEqual(0, Object.getOwnPropertyNames(result.parameters).length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http HEAD
////////////////////////////////////////////////////////////////////////////////

    testHead : function () {
      var response = internal.download(buildUrl(""), undefined, { method: "head" });

      assertEqual(200, response.code);

      assertEqual(0, response.body.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http DELETE
////////////////////////////////////////////////////////////////////////////////

    testDelete : function () {
      var result, response = internal.download(buildUrl(""), undefined, { method: "delete" });

      assertEqual(200, response.code);

      result = JSON.parse(response.body);
      assertEqual("DELETE", result.requestType);
      assertEqual(0, Object.getOwnPropertyNames(result.parameters).length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET with body
////////////////////////////////////////////////////////////////////////////////

    testGetWithBody : function () {
      try {
        internal.download(buildUrl(""), "bang!", { method: "get" });
        fail();
      }
      catch (err) {
        assertEqual(arangodb.ERROR_BAD_PARAMETER, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http HEAD with body
////////////////////////////////////////////////////////////////////////////////

    testHeadWithBody : function () {
      try {
        internal.download(buildUrl(""), "bang!", { method: "head" });
        fail();
      }
      catch (err) {
        assertEqual(arangodb.ERROR_BAD_PARAMETER, err.errorNum);
      }
    }
*/
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(DownloadSuite);

return jsunity.done();

