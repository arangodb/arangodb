/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNotNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dump/reload
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
/// @author Lukas Doomen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity"),
  arangodb = require("@arangodb"),
  db = arangodb.db;

require("@arangodb/test-helper").waitForFoxxInitialized();

function TemplateMiddlewareSpec () {
  'use strict';
  var TemplateMiddleware, templateMiddleware, templateCollection, request, response, options, next, error;

  return {
    setUp: function () {
      request = {};
      response = {};
      options = {};
      next = function () {};
      TemplateMiddleware = require("@arangodb/foxx/legacy/template_middleware").TemplateMiddleware;
      db._drop("templateTest");
      templateCollection = db._create("templateTest");
    },

    tearDown: function () {
      db._drop("templateTest");
    },

    testRenderingATemplateWhenInitializedWithACollection: function () {
      templateCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      templateMiddleware = new TemplateMiddleware(templateCollection);
      templateMiddleware(request, response, options, next);

      response.render("simple/path", { username: "moonglum" });
      assertEqual(response.body, "hello moonglum");
      assertEqual(response.contentType, "text/plain");
    },

    testRenderingATemplateWhenInitializedWithACollectionNameOfAnExistingCollection: function () {
      templateCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      templateMiddleware = new TemplateMiddleware("templateTest");
      templateMiddleware(request, response, options, next);

      response.render("simple/path", { username: "moonglum" });
      assertEqual(response.body, "hello moonglum");
      assertEqual(response.contentType, "text/plain");
    },

    testShouldCreateCollectionIfNotFound: function () {
      db._drop("templateTest");
      templateMiddleware = new TemplateMiddleware("templateTest");
      templateMiddleware(request, response, options, next);

      assertNotNull(db._collection("templateTest"));
    },

    testRenderingATemplateWithAnUnknownTemplateEngine: function () {
      templateCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "pirateEngine"
      });

      templateMiddleware = new TemplateMiddleware(templateCollection);
      templateMiddleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Unknown template language 'pirateEngine'"));
    },

    testRenderingATemplateWithAnNotExistingTemplate: function () {
      templateMiddleware = new TemplateMiddleware(templateCollection);
      templateMiddleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Template 'simple/path' does not exist"));
    },

    testHelpers: function () {
      var a = false;

      templateCollection.save({
        path: "simple/path",
        content: "hello <%= testHelper() %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      templateMiddleware = new TemplateMiddleware(templateCollection, {
        testHelper: function() { a = true; }
      });
      templateMiddleware(request, response, options, next);

      assertFalse(a);
      response.render("simple/path", {});
      assertTrue(a);
    }
  };
}

jsunity.run(TemplateMiddlewareSpec);

return jsunity.done();
