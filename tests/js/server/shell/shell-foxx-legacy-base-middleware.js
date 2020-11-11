/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the agency communication layer
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
/// @author Lucas Doomen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

require("@arangodb/test-helper").waitForFoxxInitialized();

function BaseMiddlewareSpec () {
  'use strict';
  var request, response, options, next, baseMiddleware;

  return {
    setUp: function () {
      baseMiddleware = require("@arangodb/foxx/legacy/base_middleware").BaseMiddleware().functionRepresentation;
      request = {};
      response = {};
      options = {};
      next = function () {};
    },

    testBodyFunctionAddedToRequest: function () {
      request.requestBody = JSON.stringify({test: 123});
      baseMiddleware(request, response, options, next);
      assertEqual(request.body(), {test: 123});
    },

    testRawBodyFunctionAddedToRequest: function () {
      request.requestBody = JSON.stringify({test: 123});
      baseMiddleware(request, response, options, next);
      assertEqual(request.rawBody(), JSON.stringify({test: 123}));
    },

    testParamFunctionReturnsUrlParameters: function () {
      request.urlParameters = {a: 1};
      baseMiddleware(request, response, options, next);
      assertEqual(request.params("a"), 1);
    },

    testParamFunctionReturnsParameters: function () {
      request.parameters = {a: 1};
      baseMiddleware(request, response, options, next);
      assertEqual(request.params("a"), 1);
    },

    testParamFunctionReturnsAllParams: function () {
      request.urlParameters = {a: 1};
      request.parameters = {b: 2};
      baseMiddleware(request, response, options, next);
      assertEqual(request.params("a"), 1);
      assertEqual(request.params("b"), 2);
    },

    testStatusFunctionAddedToResponse: function () {
      baseMiddleware(request, response, options, next);

      response.status(200);
      assertEqual(response.responseCode, 200);
    },

    testSetFunctionAddedToResponse: function () {
      baseMiddleware(request, response, options, next);

      response.set("Content-Length", "123");
      assertEqual(response.headers["content-length"], "123");

      response.set("Content-Type", "text/plain");
      assertEqual(response.contentType, "text/plain");
    },

    testSetFunctionTakingAnObjectAddedToResponse: function () {
      baseMiddleware(request, response, options, next);

      response.set({
        "Content-Length": "123",
        "Content-Type": "text/plain"
      });

      assertEqual(response.headers["content-length"], "123");
      assertEqual(response.contentType, "text/plain");
    },

    testJsonFunctionAddedToResponse: function () {
      var rawObject = {test: "123"};

      baseMiddleware(request, response, options, next);

      response.json(rawObject);

      assertEqual(response.body, JSON.stringify(rawObject));
      assertEqual(response.contentType, "application/json");
    },

    testTemplateFunctionAddedToResponse: function () {
      var error;

      baseMiddleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("No template collection has been provided when creating a new FoxxController"));
    },

    testMiddlewareCallsTheAction: function () {
      var actionWasCalled = false;

      next = function () {
        actionWasCalled = true;
      };

      baseMiddleware(request, response, options, next);

      assertTrue(actionWasCalled);
    }
  };
}

jsunity.run(BaseMiddlewareSpec);

return jsunity.done();
