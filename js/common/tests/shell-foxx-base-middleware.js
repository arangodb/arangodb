require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  arangodb = require("org/arangodb"),
  db = arangodb.db;

function BaseMiddlewareWithoutTemplateSpec () {
  var BaseMiddleware, request, response, options, next;

  return {
    setUp: function () {
      baseMiddleware = require("org/arangodb/foxx/base_middleware").BaseMiddleware().functionRepresentation;
      request = {};
      response = {};
      options = {};
      next = function () {};
    },

    testBodyFunctionAddedToRequest: function () {
      request.requestBody = "test";
      baseMiddleware(request, response, options, next);
      assertEqual(request.body(), "test");
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

      assertEqual(error, new Error("No template collection has been provided when creating a new FoxxApplication"));
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

function BaseMiddlewareWithTemplateSpec () {
  var BaseMiddleware, request, response, options, next;

  return {
    setUp: function () {
      request = {};
      response = {};
      options = {};
      next = function () {};
      BaseMiddleware = require("org/arangodb/foxx/base_middleware").BaseMiddleware;
    },

    testRenderingATemplate: function () {
      var myCollection, middleware;

      db._drop("templateTest");
      myCollection = db._create("templateTest");

      myCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      middleware = new BaseMiddleware(myCollection).functionRepresentation;
      middleware(request, response, options, next);

      response.render("simple/path", { username: "moonglum" });
      assertEqual(response.body, "hello moonglum");
      assertEqual(response.contentType, "text/plain");
    },

    testRenderingATemplateWithAnUnknownTemplateEngine: function () {
      var myCollection, error, middleware;

      db._drop("templateTest");
      myCollection = db._create("templateTest");

      myCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "pirateEngine"
      });

      middleware = new BaseMiddleware(myCollection).functionRepresentation;
      middleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Unknown template language 'pirateEngine'"));
    },

    testRenderingATemplateWithAnNotExistingTemplate: function () {
      var myCollection, error, middleware;

      db._drop("templateTest");
      myCollection = db._create("templateTest");

      middleware = new BaseMiddleware(myCollection).functionRepresentation;
      middleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Template 'simple/path' does not exist"));
    }
  };
}

jsunity.run(BaseMiddlewareWithoutTemplateSpec);
jsunity.run(BaseMiddlewareWithTemplateSpec);

return jsunity.done();
