require("internal").flushModuleCache();

var jsunity = require("jsunity");

var console = require("console");
var arangodb = require("org/arangodb");
var Frank = require("org/arangodb/frank").Frank;
var db = arangodb.db;

function CreateFrankSpec () {
  return {
    testCreationWithoutParameters: function () {
      var app = new Frank(),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertUndefined(routingInfo.urlPrefix);
    },

    testCreationWithUrlPrefix: function () {
      var app = new Frank({urlPrefix: "/frankentest"}),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertEqual(routingInfo.urlPrefix, "/frankentest");
    },

    testCreationWithTemplateCollectionIfCollectionDoesntExist: function () {
      var app, routingInfo, templateCollection;

      db._drop("frankentest");
      app = new Frank({templateCollection: "frankentest"});
      routingInfo = app.routingInfo;
      templateCollection = db._collection("frankentest");

      assertEqual(routingInfo.routes.length, 0);
      assertNotNull(templateCollection);
      assertEqual(routingInfo.templateCollection, templateCollection);
    },

    testCreationWithTemplateCollectionIfCollectionDoesExist: function () {
      var app, routingInfo, templateCollection;

      db._drop("frankentest");
      db._create("frankentest");
      app = new Frank({templateCollection: "frankentest"});
      routingInfo = app.routingInfo;
      templateCollection = db._collection("frankentest");

      assertEqual(routingInfo.routes.length, 0);
      assertNotNull(templateCollection);
      assertEqual(routingInfo.templateCollection, templateCollection);
    }
  };
}

function SetRoutesFrankSpec () {
  var app;

  return {
    setUp: function () {
      app = new Frank();
    },

    testSettingRoutesWithoutConstraint: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.get('/simple/route', myFunc);
      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.match, '/simple/route');
      assertUndefined(routes[0].url.constraint);
    },

    testSettingRoutesWithConstraint: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes,
        constraint = { test: "/[a-z]+/" };

      app.get('/simple/route', { constraint: constraint }, myFunc);
      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.constraint, constraint);
    },

    testSetMethodToHead: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.head('/simple/route', myFunc);
      assertEqual(routes[0].url.methods, ["head"]);
    },

    testSetMethodToGet: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.get('/simple/route', myFunc);
      assertEqual(routes[0].url.methods, ["get"]);
    },

    testSetMethodToPost: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.post('/simple/route', myFunc);
      assertEqual(routes[0].url.methods, ["post"]);
    },

    testSetMethodToPut: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.put('/simple/route', myFunc);
      assertEqual(routes[0].url.methods, ["put"]);
    },

    testSetMethodToPatch: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.patch('/simple/route', myFunc);
      assertEqual(routes[0].url.methods, ["patch"]);
    },

    testSetMethodToDelete: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.delete('/simple/route', myFunc);
      assertEqual(routes[0].url.methods, ["delete"]);
    },

    testRefuseRoutesWithRoutesThatAreNumbers: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes,
        error;

      try {
        app.get(2, myFunc);
      } catch(e) {
        error = e;
      }
      assertEqual(error, "URL has to be a String");
      assertEqual(routes.length, 0);
    },

    testRefuseRoutesWithRoutesThatAreRegularExpressions: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes,
        error;

      try {
        app.get(/[a-z]*/, myFunc);
      } catch(e) {
        error = e;
      }
      assertEqual(error, "URL has to be a String");
      assertEqual(routes.length, 0);
    },

    testRefuseRoutesWithRoutesThatAreArrays: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes,
        error;

      try {
        app.get(["/a", "/b"], myFunc);
      } catch(e) {
        error = e;
      }
      assertEqual(error, "URL has to be a String");
      assertEqual(routes.length, 0);
    },

    testSettingHandlers: function () {
      var myFunc = function a (b, c) { return b + c;},
        myFuncString = "function a(b, c) { return b + c;}",
        routes = app.routingInfo.routes,
        action;

      app.get('/simple/route', myFunc);

      action = routes[0].action;
      assertEqual(routes.length, 1);
      assertEqual(action.callback, myFuncString);
    }
  };
}

function BaseMiddlewareWithoutTemplateSpec () {
  var baseMiddleware, request, response, options, next;

  return {
    setUp: function () {
      baseMiddleware = new require("org/arangodb/frank").BaseMiddleware();
      request = {};
      response = {};
      options = {};
      next = function () {};
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

      assertEqual(error, "No template collection has been provided when creating a new Frank");
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
      BaseMiddleware = new require("org/arangodb/frank").BaseMiddleware;
    },

    testRenderingATemplate: function () {
      var myCollection, middleware;

      db._drop("templateTest");
      myCollection = db._create("templateTest");

      myCollection.save({
        path: "simple/path",
        content: "hallo <%= username %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      middleware = new BaseMiddleware(myCollection);
      middleware(request, response, options, next);

      response.render("simple/path", { username: "moonglum" });
      assertEqual(response.body, "hallo moonglum");
      assertEqual(response.contentType, "text/plain");
    },

    testRenderingATemplateWithAnUnknownTemplateEngine: function () {
      var myCollection, error, middleware;

      db._drop("templateTest");
      myCollection = db._create("templateTest");

      myCollection.save({
        path: "simple/path",
        content: "hallo <%= username %>",
        contentType: "text/plain",
        templateLanguage: "pirateEngine"
      });

      middleware = new BaseMiddleware(myCollection);
      middleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, "Unknown template language 'pirateEngine'");
    },

    testRenderingATemplateWithAnNotExistingTemplate: function () {
      var myCollection, error, middleware;

      db._drop("templateTest");
      myCollection = db._create("templateTest");

      middleware = new BaseMiddleware(myCollection);
      middleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, "Template 'simple/path' does not exist");
    }
  };
}

jsunity.run(CreateFrankSpec);
jsunity.run(SetRoutesFrankSpec);
jsunity.run(BaseMiddlewareWithoutTemplateSpec);
jsunity.run(BaseMiddlewareWithTemplateSpec);

return jsunity.done();
