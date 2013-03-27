require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  console = require("console"),
  arangodb = require("org/arangodb"),
  FoxxApplication = require("org/arangodb/foxx").FoxxApplication,
  db = arangodb.db;

function CreateFoxxApplicationSpec () {
  return {
    testCreationWithoutParameters: function () {
      var app = new FoxxApplication(),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertEqual(routingInfo.urlPrefix, "");
    },

    testCreationWithURLPrefix: function () {
      var app = new FoxxApplication({urlPrefix: "/wiese"}),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertEqual(routingInfo.urlPrefix, "/wiese");
    },

    testCreationWithTemplateCollectionIfCollectionDoesntExist: function () {
      var app, routingInfo, templateCollection;

      db._drop("wiese");
      app = new FoxxApplication({templateCollection: "wiese"});
      routingInfo = app.routingInfo;
      templateCollection = db._collection("wiese");

      assertEqual(routingInfo.routes.length, 0);
      assertNotNull(templateCollection);
      assertEqual(app.templateCollection, templateCollection);
    },

    testCreationWithTemplateCollectionIfCollectionDoesExist: function () {
      var app, routingInfo, templateCollection;

      db._drop("wiese");
      db._create("wiese");
      app = new FoxxApplication({templateCollection: "wiese"});
      routingInfo = app.routingInfo;
      templateCollection = db._collection("wiese");

      assertEqual(routingInfo.routes.length, 0);
      assertNotNull(templateCollection);
      assertEqual(app.templateCollection, templateCollection);
    },

    testAdditionOfBaseMiddlewareInRoutingInfo: function () {
      var app = new FoxxApplication(),
        routingInfo = app.routingInfo,
        hopefully_base = routingInfo.middleware[0];

      assertEqual(routingInfo.middleware.length, 1);
      assertEqual(hopefully_base.url.match, "/*");
    }
  };
}

function SetRoutesFoxxApplicationSpec () {
  var app;

  return {
    setUp: function () {
      app = new FoxxApplication();
    },

    testSettingRoutes: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.get('/simple/route', myFunc);
      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.match, '/simple/route');
      assertUndefined(routes[0].url.constraint);
    },

    testSetMethodToHead: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.head('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'HEAD');
      assertEqual(routes[0].url.methods, ["head"]);
    },

    testSetMethodToGet: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.get('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'GET');
      assertEqual(routes[0].url.methods, ["get"]);
    },

    testSetMethodToPost: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.post('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'POST');
      assertEqual(routes[0].url.methods, ["post"]);
    },

    testSetMethodToPut: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.put('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'PUT');
      assertEqual(routes[0].url.methods, ["put"]);
    },

    testSetMethodToPatch: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.patch('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'PATCH');
      assertEqual(routes[0].url.methods, ["patch"]);
    },

    testSetMethodToDelete: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app['delete']('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'DELETE');
      assertEqual(routes[0].url.methods, ["delete"]);
    },

    testSetMethodToDeleteViaAlias: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.del('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'DELETE');
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
    },

    testStartAddsRequiresAndContext: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.requiresLibs = {
        a: 1
      };
      app.requiresModels = {
        b: 2
      };
      app.get('/simple/route', myFunc);
      app.start({
        context: "myContext"
      });

      assertEqual(app.routingInfo.routes[0].action.context, "myContext");
      assertEqual(app.routingInfo.routes[0].action.requiresLibs.a, 1);
      assertEqual(app.routingInfo.routes[0].action.requiresModels.b, 2);
    }
  };
}

function DocumentationAndConstraintsSpec () {
  var app, routes;

  return {
    setUp: function () {
      app = new FoxxApplication(),
        routes = app.routingInfo.routes;
    },

    testDefinePathParam: function () {
      app.get('/foxx/:id', function () {
        //nothing
      }).pathParam("id", {
        description: "Id of the Foxx",
        dataType: "int"
      });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.constraint.id, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "id");
      assertEqual(routes[0].docs.parameters[0].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "int");
      assertEqual(routes[0].docs.parameters[0].required, true);
    },

    testDefineMultiplePathParams: function () {
      app.get('/:foxx/:id', function () {
        //nothing
      }).pathParam("foxx", {
        description: "Kind of Foxx",
        dataType: "string"
      }).pathParam("id", {
        description: "Id of the Foxx",
        dataType: "int"
      });

      assertEqual(routes.length, 1);

      assertEqual(routes[0].url.constraint.foxx, "/.+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "foxx");
      assertEqual(routes[0].docs.parameters[0].description, "Kind of Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "string");
      assertEqual(routes[0].docs.parameters[0].required, true);

      assertEqual(routes[0].url.constraint.id, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[1].paramType, "path");
      assertEqual(routes[0].docs.parameters[1].name, "id");
      assertEqual(routes[0].docs.parameters[1].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[1].dataType, "int");
      assertEqual(routes[0].docs.parameters[1].required, true);
    },

    testDefineQueryParam: function () {
      app.get('/foxx', function () {
        //nothing
      }).queryParam("a", {
        description: "The value of an a",
        dataType: "int",
        required: false,
        allowMultiple: true
      });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].paramType, "query");
      assertEqual(routes[0].docs.parameters[0].name, "a");
      assertEqual(routes[0].docs.parameters[0].description, "The value of an a");
      assertEqual(routes[0].docs.parameters[0].dataType, "int");
      assertEqual(routes[0].docs.parameters[0].required, false);
      assertEqual(routes[0].docs.parameters[0].allowMultiple, true);
    },

    testDefineMetaData: function () {
      app.get('/foxx', function () {
        //nothing
      }).nickname("a").summary("b").notes("c");

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.nickname, "a");
      assertEqual(routes[0].docs.summary, "b");
      assertEqual(routes[0].docs.notes, "c");
    },

    testDefineSummaryRestrictedTo60Characters: function () {
      var error;

      try {
        app.get('/foxx', function () {
          //nothing
        }).summary("ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
      } catch(e) {
        error = e;
      }

      assertEqual(error, "Summary can't be longer than 60 characters");
    },

    testDefineErrorResponse: function () {
      app.get('/foxx', function () {
        //nothing
      }).errorResponse(400, "I don't understand a word you're saying");

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.errorResponses.length, 1);
      assertEqual(routes[0].docs.errorResponses[0].code, 400);
      assertEqual(routes[0].docs.errorResponses[0].reason, "I don't understand a word you're saying");
    }
  };
}

function AddMiddlewareFoxxApplicationSpec () {
  var app;

  return {
    setUp: function () {
      app = new FoxxApplication();
    },

    testAddABeforeMiddlewareForAllRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware;

      app.before(myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/*');
      assertTrue(middleware[1].action.callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(middleware[1].action.callback.indexOf("next()" > 0));
      assertTrue(middleware[1].action.callback.indexOf(String(myFunc)) <
        middleware[1].action.callback.indexOf("next()"));
    },

    testAddABeforeMiddlewareForCertainRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware;

      app.before('/fancy/path', myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/fancy/path');
      assertTrue(middleware[1].action.callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(middleware[1].action.callback.indexOf("next()" > 0));
      assertTrue(middleware[1].action.callback.indexOf(String(myFunc)) <
        middleware[1].action.callback.indexOf("next()"));
    },

    testAddAnAfterMiddlewareForAllRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware;

      app.after(myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/*');
      assertTrue(middleware[1].action.callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(middleware[1].action.callback.indexOf("next()" > 0));
      assertTrue(middleware[1].action.callback.indexOf(String(myFunc)) >
        middleware[1].action.callback.indexOf("next()"));
    },

    testAddAnAfterMiddlewareForCertainRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware;

      app.after('/fancy/path', myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/fancy/path');
      assertTrue(middleware[1].action.callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(middleware[1].action.callback.indexOf("next()" > 0));
      assertTrue(middleware[1].action.callback.indexOf(String(myFunc)) >
        middleware[1].action.callback.indexOf("next()"));
    },

    testAddTheFormatMiddlewareUsingTheShortform: function () {
      app.accepts(["json"], "json");
      assertEqual(app.routingInfo.middleware.length, 2);
      assertEqual(app.routingInfo.middleware[1].url.match, '/*');
      assertTrue(app.routingInfo.middleware[1].action.callback.indexOf("[\"json\"]") > 0);
    }
  };
}

function BaseMiddlewareWithoutTemplateSpec () {
  var BaseMiddleware, request, response, options, next;

  return {
    setUp: function () {
      baseMiddleware = require("org/arangodb/foxx").BaseMiddleware().functionRepresentation;
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

      assertEqual(error, "No template collection has been provided when creating a new FoxxApplication");
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
      BaseMiddleware = require("org/arangodb/foxx").BaseMiddleware;
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

      middleware = new BaseMiddleware(myCollection).functionRepresentation;
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

      middleware = new BaseMiddleware(myCollection).functionRepresentation;
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

      middleware = new BaseMiddleware(myCollection).functionRepresentation;
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

function ViewHelperSpec () {
  var app, Middleware, request, response, options, next;

  return {
    setUp: function () {
      app = new FoxxApplication();
      Middleware = require('org/arangodb/foxx').BaseMiddleware;
      request = {};
      response = {};
      options = {};
      next = function () {};
    },

    testDefiningAViewHelper: function () {
      var my_func = function () {};

      app.helper("testHelper", my_func);

      assertEqual(app.helperCollection.testHelper, my_func);
    },

    testUsingTheHelperInATemplate: function () {
      var a = false;

      db._drop("templateTest");
      myCollection = db._create("templateTest");

      myCollection.save({
        path: "simple/path",
        content: "hallo <%= testHelper() %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      middleware = new Middleware(myCollection, {
        testHelper: function() { a = true; }
      }).functionRepresentation;
      middleware(request, response, options, next);

      assertFalse(a);
      response.render("simple/path", {});
      assertTrue(a);
    }
  };
}

function FormatMiddlewareSpec () {
  var Middleware, middleware, request, response, options, next;

  return {
    setUp: function () {
      Middleware = require('org/arangodb/foxx').FormatMiddleware;
      request = {};
      response = {};
      options = {};
      next = function () {};
    },

    testChangesTheURLAccordingly: function () {
      request = { path: "test/1.json", headers: {} };
      middleware = new Middleware(["json"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.path, "test/1");
    },

    testRefusesFormatsThatHaveNotBeenAllowed: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      middleware = new Middleware(["json"]).functionRepresentation;
      request = { path: "test/1.html", headers: {} };
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual(response.body, "Format 'html' is not allowed.");
      assertFalse(nextCalled);
    },

    testRefuseContradictingURLAndResponseType: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      request = { path: "test/1.json", headers: {"accept": "text/html"} };
      middleware = new Middleware(["json"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual(response.body, "Contradiction between Accept Header and URL.");
      assertFalse(nextCalled);
    },

    testRefuseMissingBothURLAndResponseTypeWhenNoDefaultWasGiven: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      request = { path: "test/1", headers: {} };
      middleware = new Middleware(["json"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual(response.body, "Format 'undefined' is not allowed.");
      assertFalse(nextCalled);
    },

    testFallBackToDefaultWhenMissingBothURLAndResponseType: function () {
      request = { path: "test/1", headers: {} };
      middleware = new Middleware(["json"], "json").functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.format, "json");
      assertEqual(response.contentType, "application/json");
    },

    // JSON
    testSettingTheFormatAttributeAndResponseTypeForJsonViaURL: function () {
      request = { path: "test/1.json", headers: {} };
      middleware = new Middleware(["json"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.format, "json");
      assertEqual(response.contentType, "application/json");
    },

    testSettingTheFormatAttributeAndResponseTypeForJsonViaAcceptHeader: function () {
      request = { path: "test/1", headers: {"accept": "application/json"} };
      middleware = new Middleware(["json"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.format, "json");
      assertEqual(response.contentType, "application/json");
    },

    // HTML
    testSettingTheFormatAttributeAndResponseTypeForHtmlViaURL: function () {
      request = { path: "test/1.html", headers: {} };
      middleware = new Middleware(["html"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.format, "html");
      assertEqual(response.contentType, "text/html");
    },

    testSettingTheFormatAttributeAndResponseTypeForHtmlViaAcceptHeader: function () {
      request = { path: "test/1", headers: {"accept": "text/html"} };
      middleware = new Middleware(["html"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.format, "html");
      assertEqual(response.contentType, "text/html");
    },

    // TXT
    testSettingTheFormatAttributeAndResponseTypeForTxtViaURL: function () {
      request = { path: "test/1.txt", headers: {} };
      middleware = new Middleware(["txt"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.format, "txt");
      assertEqual(response.contentType, "text/plain");
    },

    testSettingTheFormatAttributeAndResponseTypeForTxtViaAcceptHeader: function () {
      request = { path: "test/1", headers: {"accept": "text/plain"} };
      middleware = new Middleware(["txt"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(request.format, "txt");
      assertEqual(response.contentType, "text/plain");
    }
  };
}

jsunity.run(CreateFoxxApplicationSpec);
jsunity.run(SetRoutesFoxxApplicationSpec);
jsunity.run(DocumentationAndConstraintsSpec);
jsunity.run(AddMiddlewareFoxxApplicationSpec);
jsunity.run(BaseMiddlewareWithoutTemplateSpec);
jsunity.run(BaseMiddlewareWithTemplateSpec);
jsunity.run(ViewHelperSpec);
jsunity.run(FormatMiddlewareSpec);

return jsunity.done();
