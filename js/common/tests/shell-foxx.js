require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  console = require("console"),
  arangodb = require("org/arangodb"),
  FoxxApplication = require("org/arangodb/foxx").Application,
  db = arangodb.db;

function CreateFoxxApplicationSpec () {
  return {
    testCreationWithoutParameters: function () {
      var app = new FoxxApplication({prefix: "", foxxes: []}),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertEqual(routingInfo.urlPrefix, "");
    },

    testCreationWithURLPrefix: function () {
      var app = new FoxxApplication({prefix: "", foxxes: []}, {urlPrefix: "/wiese"}),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertEqual(routingInfo.urlPrefix, "/wiese");
    },

    testAdditionOfBaseMiddlewareInRoutingInfo: function () {
      var app = new FoxxApplication({prefix: "", foxxes: []}),
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
      app = new FoxxApplication({prefix: "", foxxes: []});
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
      assertEqual(error, new Error("URL has to be a String"));
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
      assertEqual(error, new Error("URL has to be a String"));
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
      assertEqual(error, new Error("URL has to be a String"));
      assertEqual(routes.length, 0);
    }
  };
}

function DocumentationAndConstraintsSpec () {
  var app, routes;

  return {
    setUp: function () {
      app = new FoxxApplication({prefix: "", foxxes: []}),
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
    
    testDefinePathCaseParam: function () {
      app.get('/foxx/:idParam', function () {
        //nothing
      }).pathParam("idParam", {
        description: "Id of the Foxx",
        dataType: "int"
      });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.constraint.idParam, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "idParam");
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

    testDefineMultiplePathCaseParams: function () {
      app.get('/:foxxParam/:idParam', function () {
        //nothing
      }).pathParam("foxxParam", {
        description: "Kind of Foxx",
        dataType: "string"
      }).pathParam("idParam", {
        description: "Id of the Foxx",
        dataType: "int"
      });

      assertEqual(routes.length, 1);

      assertEqual(routes[0].url.constraint.foxxParam, "/.+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "foxxParam");
      assertEqual(routes[0].docs.parameters[0].description, "Kind of Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "string");
      assertEqual(routes[0].docs.parameters[0].required, true);

      assertEqual(routes[0].url.constraint.idParam, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[1].paramType, "path");
      assertEqual(routes[0].docs.parameters[1].name, "idParam");
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
      }).summary("b").notes("c");

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.nickname, "get_foxx");
      assertEqual(routes[0].docs.summary, "b");
      assertEqual(routes[0].docs.notes, "c");
    },

    testSummaryRestrictedTo60Characters: function () {
      var error;

      try {
        app.get('/foxx', function () {
          //nothing
        }).summary("ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Summary can't be longer than 60 characters"));
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
      app = new FoxxApplication({prefix: "", foxxes: []});
    },

    testAddABeforeMiddlewareForAllRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware,
        callback;

      app.before(myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/*');
      callback = String(middleware[1].action.callback);
      assertTrue(callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(callback.indexOf("next()" > 0));
      assertTrue(callback.indexOf(String(myFunc)) < callback.indexOf("next()"));
    },

    testAddABeforeMiddlewareForCertainRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware,
        callback;

      app.before('/fancy/path', myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/fancy/path');
      callback = String(middleware[1].action.callback);
      assertTrue(callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(callback.indexOf("next()" > 0));
      assertTrue(callback.indexOf(String(myFunc)) < callback.indexOf("next()"));
    },

    testAddAnAfterMiddlewareForAllRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware,
        callback;

      app.after(myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/*');
      callback = String(middleware[1].action.callback);
      assertTrue(callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(callback.indexOf("next()" > 0));
      assertTrue(callback.indexOf("func(req") > callback.indexOf("next()"));
    },

    testAddAnAfterMiddlewareForCertainRoutes: function () {
      var myFunc = function (req, res) { a = (req > res); },
        middleware = app.routingInfo.middleware,
        callback;

      app.after('/fancy/path', myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/fancy/path');
      callback = String(middleware[1].action.callback);
      assertTrue(callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(callback.indexOf("next()" > 0));
      assertTrue(callback.indexOf("func(req") > callback.indexOf("next()"));
    },

    testAddTheFormatMiddlewareUsingTheShortform: function () {
      app.accepts(["json"], "json");
      assertEqual(app.routingInfo.middleware.length, 2);
      assertEqual(app.routingInfo.middleware[1].url.match, '/*');
      assertTrue(app.routingInfo.middleware[1].action.callback.indexOf("[\"json\"]") > 0);
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
      assertEqual("<" + response.body + ">", "<Error: Format 'html' is not allowed.>");
      assertFalse(nextCalled);
    },

    testRefuseContradictingURLAndResponseType: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      request = { path: "test/1.json", headers: {"accept": "text/html"} };
      middleware = new Middleware(["json"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual("<" + response.body + ">", "<Error: Contradiction between Accept Header and URL.>");
      assertFalse(nextCalled);
    },

    testRefuseMissingBothURLAndResponseTypeWhenNoDefaultWasGiven: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      request = { path: "test/1", headers: {} };
      middleware = new Middleware(["json"]).functionRepresentation;
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual("<" + response.body + ">", "<Error: Format 'undefined' is not allowed.>");
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
jsunity.run(FormatMiddlewareSpec);

return jsunity.done();
