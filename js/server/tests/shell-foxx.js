/*jshint unused: false */
/*global require, assertEqual, assertTrue, assertFalse, assertUndefined */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lucas Dohmen
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
// Stubbing and Mocking

var joi = require("joi"),
  transformRoute = require("org/arangodb/foxx/routing").__test_transformControllerToRoute,
  _ = require("underscore");

function stub() {
  return function() {};
}

// allow(x)
//   .toReceive("functionName")
//   .andReturn({ x: 1 })

function allow(obj) {
  return (new FunctionStub(obj));
}

function FunctionStub(obj) {
  this.obj = obj;
}

_.extend(FunctionStub.prototype, {
  toReceive: function (functionName) {
    this.functionName = functionName;
    this.buildFunctionStub();
    return this;
  },

  andReturn: function (returnValue) {
    this.returnValue = returnValue;
    this.buildFunctionStub();
    return this;
  },

  buildFunctionStub: function () {
    var returnValue = this.returnValue;

    this.obj[this.functionName] = function () {
      return returnValue;
    };
  }
});

/* Create a Mock Constructor
 *
 * Give the arguments you expect to mockConstructor:
 * It checks, if it was called with new and the
 * correct arguments.
 *
 * MyProto = mockConstructor(test);
 *
 * a = new MyProto(test);
 *
 * MyProto.assertIsSatisfied();
 */
function mockConstructor() {
  var expectedArguments = arguments,
    satisfied = false;
  function MockConstructor() {
    if (this.constructor === MockConstructor) {
      // Was called as a constructor
      satisfied = _.isEqual(arguments, expectedArguments);
    }
  }

  MockConstructor.assertIsSatisfied = function () {
    assertTrue(satisfied);
  };

  return MockConstructor;
}


var jsunity = require("jsunity"),
  FoxxController = require("org/arangodb/foxx").Controller,
  fakeContext,
  fakeContextWithRootElement;

fakeContext = {
  prefix: "",
  foxxes: [],
  comments: [],
  manifest: {
    rootElement: false
  },
  clearComments: stub(),
  comment: stub(),
  collectionName: stub()
};

fakeContextWithRootElement = {
  prefix: "",
  foxxes: [],
  comments: [],
  manifest: {
    rootElement: true
  },
  clearComments: stub(),
  comment: stub(),
  collectionName: stub()
};

function CreateFoxxControllerSpec () {
  return {
    testCreationWithoutParameters: function () {
      var app = new FoxxController(fakeContext),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertEqual(routingInfo.urlPrefix, "");
    },

    testCreationWithURLPrefix: function () {
      var app = new FoxxController(fakeContext, {urlPrefix: "/wiese"}),
        routingInfo = app.routingInfo;

      assertEqual(routingInfo.routes.length, 0);
      assertEqual(routingInfo.urlPrefix, "/wiese");
    },

    testAdditionOfBaseMiddlewareInRoutingInfo: function () {
      var app = new FoxxController(fakeContext),
        routingInfo = app.routingInfo,
        hopefully_base = routingInfo.middleware[0];

      assertEqual(routingInfo.middleware.length, 1);
      assertEqual(hopefully_base.url.match, "/*");
    }
  };
}

function SetRoutesFoxxControllerSpec () {
  var app;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
    },

    testSettingRoutes: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.get('/simple/route', myFunc);
      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.match, '/simple/route');
      assertUndefined(routes[0].url.constraint);
    },

    testSetMethodToHead: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.head('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'HEAD');
      assertEqual(routes[0].url.methods, ["head"]);
    },

    testSetMethodToGet: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.get('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'GET');
      assertEqual(routes[0].url.methods, ["get"]);
    },

    testSetMethodToPost: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.post('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'POST');
      assertEqual(routes[0].url.methods, ["post"]);
    },

    testSetMethodToPut: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.put('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'PUT');
      assertEqual(routes[0].url.methods, ["put"]);
    },

    testSetMethodToPatch: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.patch('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'PATCH');
      assertEqual(routes[0].url.methods, ["patch"]);
    },

    testSetMethodToDelete: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app['delete']('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'DELETE');
      assertEqual(routes[0].url.methods, ["delete"]);
    },

    testSetMethodToDeleteViaAlias: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.del('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'DELETE');
      assertEqual(routes[0].url.methods, ["delete"]);
    },

    testRefuseRoutesWithRoutesThatAreNumbers: function () {
      var myFunc = stub(),
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
      var myFunc = stub(),
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
      var myFunc = stub(),
        routes = app.routingInfo.routes,
        error;

      try {
        app.get(["/a", "/b"], myFunc);
      } catch(e) {
        error = e;
      }
      assertEqual(error, new Error("URL has to be a String"));
      assertEqual(routes.length, 0);
    },

    testAddALoginRoute: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.activateAuthentication({
        type: "cookie",
        cookieLifetime: 360000,
        cookieName: "my_cookie",
        sessionLifetime: 400
      });
      app.login('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'POST');
      assertEqual(routes[0].url.methods, ["post"]);
    },

    testRefuseLoginWhenAuthIsNotSetUp: function () {
      var myFunc = stub(),
        error;

      try {
        app.login('/simple/route', myFunc);
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Setup authentication first"));
    },

    testAddALogoutRoute: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.activateAuthentication({
        type: "cookie",
        cookieLifetime: 360000,
        cookieName: "my_cookie",
        sessionLifetime: 400
      });
      app.logout('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'POST');
      assertEqual(routes[0].url.methods, ["post"]);
    },

    testRefuseLogoutWhenAuthIsNotSetUp: function () {
      var myFunc = stub(),
        error;

      try {
        app.logout('/simple/route', myFunc);
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Setup authentication first"));
    },

    testAddADestroySessionRoute: function () {
      var myFunc = stub(),
        routes = app.routingInfo.routes;

      app.activateSessions({
        sessionStorageApp: 'sessions',
        type: 'cookie',
        cookie: {
          name: 'sid',
          secret: 'secret'
        }
      });
      app.destroySession('/simple/route', myFunc);
      assertEqual(routes[0].docs.httpMethod, 'POST');
      assertEqual(routes[0].url.methods, ["post"]);
    },

    testRefuseDestroySessionWhenSessionsAreNotSetUp: function () {
      var myFunc = stub(),
        error;

      try {
        app.destroySession('/simple/route', myFunc);
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Setup sessions first"));
    }


  };
}

function ControllerInjectionSpec () {
  return {
    testInjectFactoryFunction: function () {
      var app = new FoxxController(fakeContext),
        req = {},
        res = {},
        timesCalled = 0;

      app.addInjector({thing: function() {timesCalled++;}});

      app.get('/foxx', stub());

      var callback = transformRoute(app.routingInfo.routes[0].action);
      callback(req, res);
      callback(req, res);

      assertEqual(timesCalled, 1);
    },
    testInjectNameValue: function () {
      var app = new FoxxController(fakeContext),
        req = {},
        res = {},
        timesCalled = 0;

      app.addInjector('thing', function() {timesCalled++;});

      app.get('/foxx', stub());

      var callback = transformRoute(app.routingInfo.routes[0].action);
      callback(req, res);
      callback(req, res);

      assertEqual(timesCalled, 1);
    },
    testInjectOverwrite: function () {
      var app = new FoxxController(fakeContext),
        req = {},
        res = {},
        wrongFuncCalled = false,
        timesCalled = 0;

      app.addInjector({thing: function() {wrongFuncCalled = true;}});
      app.addInjector({thing: function() {timesCalled++;}});

      app.get('/foxx', stub());

      var callback = transformRoute(app.routingInfo.routes[0].action);
      callback(req, res);
      callback(req, res);

      assertFalse(wrongFuncCalled);
      assertEqual(timesCalled, 1);
    },
    testInjectInRoute: function () {
      var app = new FoxxController(fakeContext),
        req = {},
        res = {},
        calledA = false,
        calledB = false,
        calledC = false;

      app.addInjector({thing: function() {calledA = true;}});

      app.get('/foxx', function () {
        app.addInjector({
          thing: function() {calledB = true;},
          other: function() {calledC = true;}
        });
      });

      var callback = transformRoute(app.routingInfo.routes[0].action);
      callback(req, res);
      callback(req, res);
      callback(req, res);

      assertTrue(calledA);
      assertFalse(calledB);
      assertTrue(calledC);
    },
    testInjectResultPassedThrough: function () {
      var app = new FoxxController(fakeContext),
        req = {},
        res = {},
        called = false;

      app.addInjector({thing: function() {return 'value';}});

      app.get('/foxx', function (req, res, injected) {
        assertEqual(injected.thing, 'value');
        called = true;
      });

      var callback = transformRoute(app.routingInfo.routes[0].action);
      callback(req, res);

      assertTrue(called);
    },
    testInjectSimpleValues: function () {
      var app = new FoxxController(fakeContext),
        req = {},
        res = {},
        called = false;

      var injectors = {
        obj: {a: 0, b: 1},
        arr: ['one', 'two'],
        str: 'hello',
        num: 42
      };

      app.addInjector(injectors);

      app.get('/foxx', function (req, res, injected) {
        assertEqual(typeof injected.obj, 'object');
        assertEqual(typeof injected.arr, 'object');
        assertEqual(typeof injected.str, 'string');
        assertEqual(typeof injected.num, 'number');
        assertTrue(Array.isArray(injected.arr));
        assertEqual(injected.obj, injectors.obj);
        assertEqual(injected.arr, injectors.arr);
        assertEqual(injected.str, injectors.str);
        assertEqual(injected.num, injectors.num);
        called = true;
      });

      var callback = transformRoute(app.routingInfo.routes[0].action);
      callback(req, res);

      assertTrue(called);
    }
  };
}

function DocumentationAndConstraintsSpec () {
  var app, routes, models, Model;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
      routes = app.routingInfo.routes;
      models = app.models;
      Model = require('org/arangodb/foxx').Model;
    },

    testDefinePathParam: function () {
      var constraint = joi.number().integer().description("Id of the Foxx"),
        context = app.get('/foxx/:id', function () {
          //nothing
        }).pathParam("id", {
          type: constraint
        });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.constraint.id, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "id");
      assertEqual(routes[0].docs.parameters[0].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(context.constraints.urlParams, {id: constraint});
    },

    testDefinePathParamShorthand: function () {
      var constraint = joi.number().integer().description("Id of the Foxx"),
        context = app.get('/foxx/:id', function () {
          //nothing
        }).pathParam("id", constraint);

      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.constraint.id, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "id");
      assertEqual(routes[0].docs.parameters[0].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(context.constraints.urlParams, {id: constraint});
    },

    testDefinePathCaseParam: function () {
      var constraint = joi.number().integer().description("Id of the Foxx"),
        context = app.get('/foxx/:idParam', function () {
          //nothing
        }).pathParam("idParam", {
          type: constraint
        });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.constraint.idParam, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "idParam");
      assertEqual(routes[0].docs.parameters[0].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(context.constraints.urlParams, {idParam: constraint});
    },

    testDefineMultiplePathParams: function () {
      var foxxConstraint = joi.string().description("Kind of Foxx"),
        idConstraint = joi.number().integer().description("Id of the Foxx"),
        context = app.get('/:foxx/:id', function () {
          //nothing
        }).pathParam("foxx", {
          type: foxxConstraint
        }).pathParam("id", {
          type: idConstraint
        });

      assertEqual(routes.length, 1);

      assertEqual(routes[0].url.constraint.foxx, "/[^/]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "foxx");
      assertEqual(routes[0].docs.parameters[0].description, "Kind of Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "string");

      assertEqual(routes[0].url.constraint.id, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[1].paramType, "path");
      assertEqual(routes[0].docs.parameters[1].name, "id");
      assertEqual(routes[0].docs.parameters[1].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[1].dataType, "integer");

      assertEqual(context.constraints.urlParams, {foxx: foxxConstraint, id: idConstraint});
    },

    testDefineMultiplePathCaseParams: function () {
      var foxxConstraint = joi.string().description("Kind of Foxx"),
        idConstraint = joi.number().integer().description("Id of the Foxx"),
        context = app.get('/:foxxParam/:idParam', function () {
          //nothing
        }).pathParam("foxxParam", {
          type: foxxConstraint
        }).pathParam("idParam", {
          type: idConstraint
        });

      assertEqual(routes.length, 1);

      assertEqual(routes[0].url.constraint.foxxParam, "/[^/]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "foxxParam");
      assertEqual(routes[0].docs.parameters[0].description, "Kind of Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "string");

      assertEqual(routes[0].url.constraint.idParam, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[1].paramType, "path");
      assertEqual(routes[0].docs.parameters[1].name, "idParam");
      assertEqual(routes[0].docs.parameters[1].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[1].dataType, "integer");

      assertEqual(context.constraints.urlParams, {foxxParam: foxxConstraint, idParam: idConstraint});
    },

    testDefineQueryParam: function () {
      var constraint = joi.number().integer().description("The value of an a"),
        context = app.get('/foxx', function () {
          //nothing
        }).queryParam("a", {
          type: constraint
        });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].paramType, "query");
      assertEqual(routes[0].docs.parameters[0].name, "a");
      assertEqual(routes[0].docs.parameters[0].description, "The value of an a");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(routes[0].docs.parameters[0].required, false);
      assertEqual(routes[0].docs.parameters[0].allowMultiple, false);
      assertEqual(context.constraints.queryParams, {a: constraint});
    },

    testDefineQueryParamWithOverrides: function () {
      var constraint = joi.number().integer(),
        context = app.get('/foxx', function () {
          //nothing
        }).queryParam("a", {
          type: constraint,
          description: "The value of an a",
          allowMultiple: true,
          required: true
        });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].paramType, "query");
      assertEqual(routes[0].docs.parameters[0].name, "a");
      assertEqual(routes[0].docs.parameters[0].description, "The value of an a");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(routes[0].docs.parameters[0].required, true, "param required not equal?");
      assertEqual(routes[0].docs.parameters[0].allowMultiple, true, "allows multiple parameters?");
      assertEqual(context.constraints.queryParams, {
        a: constraint
        .description("The value of an a")
        .meta({allowMultiple: true})
        .required()
      });
    },

    testDefineQueryParamShorthand: function () {
      var constraint = joi.number().integer()
      .description("The value of an a")
      .meta({allowMultiple: true}),
        context = app.get('/foxx', function () {
          //nothing
        }).queryParam("a", constraint);

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].paramType, "query");
      assertEqual(routes[0].docs.parameters[0].name, "a");
      assertEqual(routes[0].docs.parameters[0].description, "The value of an a");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(routes[0].docs.parameters[0].required, false);
      assertEqual(routes[0].docs.parameters[0].allowMultiple, true);
      assertEqual(context.constraints.queryParams, {a: constraint});
    },

    testAddBodyParam: function () {
      var paramName = stub(),
        description = stub(),
        ModelPrototype = stub(),
        jsonSchema = { id: 'a', required: [], properties: {} };

      allow(ModelPrototype)
        .toReceive("toJSONSchema")
        .andReturn(jsonSchema);

      app.get('/foxx', function () {
        //nothing
      }).bodyParam(paramName, {
        description: description,
        type: ModelPrototype
      });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].name, paramName);
      assertEqual(routes[0].docs.parameters[0].paramType, "body");
      assertEqual(routes[0].docs.parameters[0].description, description);
      var token = routes[0].docs.parameters[0].dataType;
      assertEqual(app.models.hasOwnProperty(token), true);
    },

    testAddBodyParamWithMultipleItems: function () {
      var paramName = stub(),
        description = stub(),
        ModelPrototype = stub(),
        jsonSchema = { id: 'a', required: [], properties: {} };

      allow(ModelPrototype)
        .toReceive("toJSONSchema")
        .andReturn(jsonSchema);

      app.get('/foxx', function () {
        //nothing
      }).bodyParam(paramName, {
        description: description,
        type: [ModelPrototype]
      });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].name, paramName);
      assertEqual(routes[0].docs.parameters[0].paramType, "body");
      assertEqual(routes[0].docs.parameters[0].description, description);
      var token = routes[0].docs.parameters[0].dataType;
      assertEqual(app.models.hasOwnProperty(token), true);
    },

    testDefineBodyParamAddsJSONSchemaToModels: function () {
      var paramName = stub(),
        description = stub(),
        ModelPrototype = stub(),
        jsonSchema = { id: 'a', required: [], properties: {} };

      allow(ModelPrototype)
        .toReceive("toJSONSchema")
        .andReturn(jsonSchema);

      app.get('/foxx', function () {
        //nothing
      }).bodyParam(paramName, {
        description: description,
        type: ModelPrototype
      });

      var token = routes[0].docs.parameters[0].dataType;
      assertEqual(app.models[token], jsonSchema);
    },

    testSetParamForBodyParam: function () {
      var req = { parameters: {} },
        res = {},
        paramName = stub(),
        description = stub(),
        requestBody = stub(),
        ModelPrototype = stub(),
        jsonSchemaId = stub(),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      ModelPrototype = mockConstructor(requestBody);
      ModelPrototype.toJSONSchema = function () { return { id: jsonSchemaId }; };

      app.get('/foxx', function (providedReq) {
        called = (providedReq.parameters[paramName] instanceof ModelPrototype);
      }).bodyParam(paramName, {
        description: description,
        type: ModelPrototype
      });
      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
      ModelPrototype.assertIsSatisfied();
    },

    testSetParamForJoiBodyParam: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = {x: 1},
        schema = {x: joi.number().integer().required()},
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = _.isEqual(providedReq.parameters[paramName], {x: 1});
      }).bodyParam(paramName, {
        description: description,
        type: schema
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
    },

    testSetParamForPureJoiBodyParam: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = {x: 1},
        schema = joi.object().keys({x: joi.number().integer().required()}),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = _.isEqual(providedReq.parameters[paramName], {x: 1});
      }).bodyParam(paramName, {
        description: description,
        type: schema
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
    },

    testSetParamForPureJoiBodyParamWithEmbeddedArgs: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = 'o hi mark',
        requestBody = {x: 1},
        schema = joi.object().keys({x: joi.number().integer().required()}),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = _.isEqual(providedReq.parameters[paramName], {x: 1});
      }).bodyParam(paramName, schema.description(description));

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
    },

    testSetParamForExoticJoiBodyParam: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = [{x: 1}],
        schema = joi.array().items({x: joi.number().integer().required()}),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = _.isEqual(providedReq.parameters[paramName], [{x: 1}]);
      }).bodyParam(paramName, {
        description: description,
        type: schema
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
    },

    testSetParamForInvalidJoiBodyParamWithAllowInvalid: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = 'banana',
        schema = joi.array().items({x: joi.number().integer().required()}),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = _.isEqual(providedReq.parameters[paramName], 'banana');
      }).bodyParam(paramName, {
        description: description,
        allowInvalid: true,
        type: schema
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
    },

    testSetParamForInvalidJoiBodyParamWithEmbeddedAllowInvalid: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = 'banana',
        schema = joi.array().items({x: joi.number().integer().required()}),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = _.isEqual(providedReq.parameters[paramName], 'banana');
      }).bodyParam(paramName, {
        description: description,
        type: schema.meta({allowInvalid: true})
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
    },

    testSetParamForInvalidJoiBodyParam: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = 'banana',
        schema = joi.array().items({x: joi.number().integer().required()}),
        called = false,
        thrown = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function () {
        called = true;
      }).bodyParam(paramName, {
        description: description,
        type: schema
      });

      try {
        var callback = transformRoute(routes[0].action);
        callback(req, res);
      } catch(e) {
        thrown = true;
      }

      assertTrue(thrown);
      assertFalse(called);
    },

    testSetParamForUndocumentedBodyParam: function () {
      var reqBody = '{"foo": "bar"}',
        req = {
          parameters: {},
          body: function () { return JSON.parse(this.rawBody()); },
          rawBody: function () { return reqBody; }
        },
        res = {},
        receivedParam = false,
        receivedRawBody = null;

      app.post('/foxx', function (providedReq) {
        receivedParam = providedReq.parameters["undocumented body"];
        receivedRawBody = providedReq.rawBody();
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(receivedParam instanceof Model);
      assertEqual(receivedParam.forDB(), {foo: "bar"});
      assertEqual(receivedRawBody, reqBody);
    },

    testSetParamForNonJsonUndocumentedBodyParam: function () {
      var reqBody = '{:foo "bar"}',
        req = {
          parameters: {},
          body: function () { return JSON.parse(this.rawBody()); },
          rawBody: function () { return reqBody; }
        },
        res = {},
        receivedParam = false,
        receivedRawBody = null;

      app.post('/foxx', function (providedReq) {
        receivedParam = providedReq.parameters["undocumented body"];
        receivedRawBody = providedReq.rawBody();
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(receivedParam instanceof Model);
      assertEqual(receivedParam.forDB(), {});
      assertEqual(receivedRawBody, reqBody);
    },

    testSetParamForBodyParamWithMultipleItems: function () {
      var req = { parameters: {} },
        res = {},
        paramName = stub(),
        description = stub(),
        rawElement = stub(),
        requestBody = [rawElement],
        ModelPrototype = stub(),
        jsonSchemaId = stub(),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      ModelPrototype = mockConstructor(rawElement);
      ModelPrototype.toJSONSchema = function () { return { id: jsonSchemaId }; };

      app.get('/foxx', function (providedReq) {
        called = (providedReq.parameters[paramName][0] instanceof ModelPrototype);
      }).bodyParam(paramName, {
        description: description,
        type: [ModelPrototype]
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
      ModelPrototype.assertIsSatisfied();
    },

    testDocumentationForErrorResponse: function () {
      var CustomErrorClass = stub();

      app.get('/foxx', function () {
        //nothing
      }).errorResponse(CustomErrorClass, 400, "I don't understand a word you're saying");

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.errorResponses.length, 1);
      assertEqual(routes[0].docs.errorResponses[0].code, 400);
      assertEqual(routes[0].docs.errorResponses[0].reason, "I don't understand a word you're saying");
    },

    testCatchesDefinedError: function () {
      var CustomErrorClass = stub(),
        req = {},
        res,
        code = 400,
        reason = "This error was really... something!",
        statusWasCalled = false,
        jsonWasCalled = false,
        passedRequestAndResponse = false;

      res = {
        status: function (givenCode) {
          statusWasCalled = (givenCode === code);
        },
        json: function (givenBody) {
          jsonWasCalled = (givenBody.error === reason);
        }
      };

      app.get('/foxx', function (providedReq, providedRes) {
        if (providedReq === req && providedRes === res) {
          passedRequestAndResponse = true;
        }
        throw new CustomErrorClass();
      }).errorResponse(CustomErrorClass, code, reason);

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(statusWasCalled);
      assertTrue(jsonWasCalled);
      assertTrue(passedRequestAndResponse);
    },

    testCatchesDefinedErrorWithCustomFunction: function () {
      var jsonWasCalled = false,
        req = {},
        res,
        code = 400,
        reason = "This error was really... something!",
        CustomErrorClass = stub();

      res = {
        status: stub(),
        json: function (givenBody) {
          jsonWasCalled = givenBody.success;
        }
      };

      app.get('/foxx', function (providedReq, providedRes) {
        throw new CustomErrorClass();
      }).errorResponse(CustomErrorClass, code, reason, function (e) {
        if (e instanceof CustomErrorClass) {
          return { success: "true" };
        }
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(jsonWasCalled);
    },

    testControllerWideErrorResponse: function () {
      var CustomErrorClass = stub();

      app.allRoutes.errorResponse(CustomErrorClass, 400, "I don't understand a word you're saying");

      app.get('/foxx', function () {
        //nothing
      });

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.errorResponses.length, 1);
      assertEqual(routes[0].docs.errorResponses[0].code, 400);
      assertEqual(routes[0].docs.errorResponses[0].reason, "I don't understand a word you're saying");
    }
  };
}

function AddMiddlewareFoxxControllerSpec () {
  // TODO Make these tests less insane. String(fn) is ridiculous.
  var app;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
    },

    testAddABeforeMiddlewareForAllRoutes: function () {
      var myFunc = function (req, res) { var a = (req > res); },
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
      var myFunc = function (req, res) { var a = (req > res); },
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
      var myFunc = function (req, res) { var a = (req > res); },
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
      var myFunc = function (req, res) { var a = (req > res); },
        middleware = app.routingInfo.middleware,
        callback;

      app.after('/fancy/path', myFunc);

      assertEqual(middleware.length, 2);
      assertEqual(middleware[1].url.match, '/fancy/path');
      callback = String(middleware[1].action.callback);
      assertTrue(callback.indexOf((String(myFunc) + "(req, res)") > 0));
      assertTrue(callback.indexOf("next()" > 0));
      assertTrue(callback.indexOf("func(req") > callback.indexOf("next()"));
    }
  };
}

function CommentDrivenDocumentationSpec () {
  var app, routingInfo, noop;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
      routingInfo = app.routingInfo;
      noop = stub();
    },

    testSettingTheSummary: function () {
      fakeContext.comments = [
        "Get all the foxes",
        "A function to get all foxes from the database",
        "in a good way."
      ];

      app.get('/simple/route', noop);

      assertEqual(routingInfo.routes[0].docs.summary, "Get all the foxes");
    },

    testSettingTheNotes: function () {
      fakeContext.comments = [
        "Get all the foxes",
        "A function to get all foxes from the database",
        "in a good way."
      ];

      app.get('/simple/route', noop);

      assertEqual(routingInfo.routes[0].docs.notes, "A function to get all foxes from the database\nin a good way.");
    },

    testSettingTheSummaryWithAnEmptyFirstLine: function () {
      fakeContext.comments = [
        "",
        "Get all the foxes"
      ];

      app.get('/simple/route', noop);

      assertEqual(routingInfo.routes[0].docs.summary, "Get all the foxes");
    },

    testCleanUpCommentsAfterwards: function () {
      var clearCommentsWasCalled = false;
      fakeContext.clearComments = function () { clearCommentsWasCalled = true; };
      fakeContext.comments = [
        "Get all the foxes",
        "A function to get all foxes from the database",
        "in a good way."
      ];

      app.get('/simple/route', noop);
      assertTrue(clearCommentsWasCalled);
    },

    testSetBothToEmptyStringsIfTheJSDocWasEmpty: function () {
      fakeContext.comments = [
        "",
        "",
        ""
      ];

      app.get('/simple/route', noop);
      assertEqual(routingInfo.routes[0].docs.summary, "");
      assertEqual(routingInfo.routes[0].docs.notes, "");
    }
  };
}

function SetupAuthorization () {
  var app;

  return {
    testWorksWithAllParameters: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateAuthentication({
          type: "cookie",
          cookieLifetime: 360000,
          cookieName: "mycookie",
          sessionLifetime: 600
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testRefusesUnknownAuthTypes: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateAuthentication({
          type: "brainwave",
          cookieLifetime: 360000,
          cookieName: "mycookie",
          sessionLifetime: 600
        });
      } catch (e) {
        err = e;
      }

      assertEqual(err.message, "Currently only the following auth types are supported: cookie");
    },

    testRefusesMissingCookieLifetime: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateAuthentication({
          type: "cookie",
          cookieName: "mycookie",
          sessionLifetime: 600
        });
      } catch (e) {
        err = e;
      }

      assertEqual(err.message, "Please provide the cookieLifetime");
    },

    testRefusesMissingCookieName: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateAuthentication({
          type: "cookie",
          cookieLifetime: 360000,
          sessionLifetime: 600
        });
      } catch (e) {
        err = e;
      }

      assertEqual(err.message, "Please provide the cookieName");
    },

    testRefusesMissingSessionLifetime: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateAuthentication({
          type: "cookie",
          cookieName: "mycookie",
          cookieLifetime: 360000
        });
      } catch (e) {
        err = e;
      }

      assertEqual(err.message, "Please provide the sessionLifetime");
    }
  };
}

function SetupSessions () {
  var app;

  return {
    testWorksWithUnsignedCookies: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'cookie',
          cookie: {
            name: 'sid'
          }
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithUnsignedCookiesShorthand: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'cookie',
          cookie: 'sid'
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithSignedCookies: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'cookie',
          cookie: {
            name: 'sid',
            secret: 'keyboardcat'
          }
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithHeaders: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Id'
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithJwt: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: {
            secret: 'secret'
          }
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithJwtHS256: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: {
            secret: 'secret',
            algorithm: 'HS256'
          }
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithJwtNone: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: {
            algorithm: 'none'
          }
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithJwtNoneWithSecret: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: {
            algorithm: 'none',
            secret: 'secret'
          }
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithJwtUnverified: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: {
            verify: false
          }
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithJwtShorthand: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: 'secret'
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testWorksWithJwtNoneShorthand: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: true
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    },

    testRefusesJwtAlgorithmWithSecret: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'header',
          header: 'X-Session-Token',
          jwt: {
            algorithm: 'HS256'
          }
        });
      } catch (e) {
        err = e;
      }

      assertTrue(err instanceof Error);
    },

    testRefusesUnknownSessionsTypes: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorageApp: 'sessions',
          type: 'magic',
          cookie: {
            name: 'sid',
            secret: 'secret'
          }
        });
      } catch (e) {
        err = e;
      }

      assertTrue(err instanceof Error);
    }
  };
}

function FoxxControllerWithRootElement () {
  var app, routes;

  return {
    setUp: function () {
      app = new FoxxController(fakeContextWithRootElement);
      routes = app.routingInfo.routes;
    },

    testBodyParamWithOneElement: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'myBodyParam',
        description = stub(),
        rawElement = stub(),
        requestBody = { myBodyParam: rawElement },
        ModelPrototype = stub(),
        jsonSchemaId = stub(),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      ModelPrototype = mockConstructor(rawElement);
      ModelPrototype.toJSONSchema = function () { return { id: jsonSchemaId }; };

      app.get('/foxx', function (providedReq) {
        called = (providedReq.parameters[paramName] instanceof ModelPrototype);
      }).bodyParam(paramName, {
        description: description,
        type: ModelPrototype
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
      ModelPrototype.assertIsSatisfied();
    },

    testBodyParamWithMultipleElement: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'myBodyParam',
        description = stub(),
        rawElement = stub(),
        requestBody = { myBodyParam: [rawElement] },
        ModelPrototype = stub(),
        jsonSchemaId = stub(),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      ModelPrototype = mockConstructor(rawElement);
      ModelPrototype.toJSONSchema = function () { return { id: jsonSchemaId }; };

      app.get('/foxx', function (providedReq) {
        called = (providedReq.parameters[paramName][0] instanceof ModelPrototype);
      }).bodyParam(paramName, {
        description: description,
        type: [ModelPrototype]
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
      ModelPrototype.assertIsSatisfied();
    }
  };
}

function ExtendFoxxControllerSpec () {
  var app, routes;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
      routes = app.routingInfo.routes;
    },

    testDocumentationBodyParameterInExtension: function() {
      var paramName = stub(),
        description = stub(),
        ModelPrototype = stub(),
        jsonSchema = { id: 'a', required: [], properties: {} };
      allow(ModelPrototype)
        .toReceive("toJSONSchema")
        .andReturn(jsonSchema);

      app.extend({
        "extension": function() {
          this.bodyParam(paramName, {
            description: description,
            type: ModelPrototype
          });
        }
      });

      app.get("/foxx", function () {
        //nothing
      }).extension();

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].name, paramName);
      assertEqual(routes[0].docs.parameters[0].paramType, "body");
      assertEqual(routes[0].docs.parameters[0].description, description);
      var token = routes[0].docs.parameters[0].dataType;
      assertEqual(app.models[token], jsonSchema);
    },

    testFunctionalityBodyParameterInExtension: function() {
      var req = { parameters: {} },
        res = {},
        paramName = stub(),
        description = stub(),
        requestBody = stub(),
        ModelPrototype = stub(),
        jsonSchemaId = stub(),
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      ModelPrototype = mockConstructor(requestBody);
      ModelPrototype.toJSONSchema = function () { return { id: jsonSchemaId }; };

      app.extend({
        "extension": function() {
          this.bodyParam(paramName, {
            description: description,
            type: ModelPrototype
          });
        }
      });

      app.get('/foxx', function (providedReq) {
        called = (providedReq.parameters[paramName] instanceof ModelPrototype);
      }).extension();

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
      ModelPrototype.assertIsSatisfied();
    },

    testPathParameterInExtension: function() {
      var constraint = joi.number().integer().description("Id of the Foxx"),
          context;

      app.extend({
        "extension": function() {
          this.pathParam("id", {
            type: constraint
          });
        }
      });

      context = app.get('/foxx/:id', function () {
        //nothing
      }).extension();

      assertEqual(routes.length, 1);
      assertEqual(routes[0].url.constraint.id, "/[0-9]+/");
      assertEqual(routes[0].docs.parameters[0].paramType, "path");
      assertEqual(routes[0].docs.parameters[0].name, "id");
      assertEqual(routes[0].docs.parameters[0].description, "Id of the Foxx");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(context.constraints.urlParams, {id: constraint});
    },

    testQueryParamterInExtension: function () {
      var constraint = joi.number().integer().description("The value of an a"),
      context;

      app.extend({
        "extension": function() {
            this.queryParam("a", {
            type: constraint
          });
        }
      });

      context = app.get('/foxx', function () {
        //nothing
      }).extension();

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.parameters[0].paramType, "query");
      assertEqual(routes[0].docs.parameters[0].name, "a");
      assertEqual(routes[0].docs.parameters[0].description, "The value of an a");
      assertEqual(routes[0].docs.parameters[0].dataType, "integer");
      assertEqual(routes[0].docs.parameters[0].required, false);
      assertEqual(routes[0].docs.parameters[0].allowMultiple, false);
      assertEqual(context.constraints.queryParams, {a: constraint});
    },

    testDocumentationErrorResponseInExtension: function () {
      var CustomErrorClass = stub();

      app.extend({
        "extension": function() {
          this.errorResponse(CustomErrorClass, 400, "I don't understand a word you're saying");
        }
      });

      app.get('/foxx', function () {
        //nothing
      }).extension();

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.errorResponses.length, 1);
      assertEqual(routes[0].docs.errorResponses[0].code, 400);
      assertEqual(routes[0].docs.errorResponses[0].reason, "I don't understand a word you're saying");
    },

    testFunctionalityErrorResponseInExtension: function () {
      var CustomErrorClass = stub(),
        req = {},
        res,
        code = 400,
        reason = "This error was really... something!",
        statusWasCalled = false,
        jsonWasCalled = false,
        passedRequestAndResponse = false;

      res = {
        status: function (givenCode) {
          statusWasCalled = (givenCode === code);
        },
        json: function (givenBody) {
          jsonWasCalled = (givenBody.error === reason);
        }
      };

      app.extend({
        "extension": function() {
          this.errorResponse(CustomErrorClass, code, reason);
        }
      });

      app.get('/foxx', function (providedReq, providedRes) {
        if (providedReq === req && providedRes === res) {
          passedRequestAndResponse = true;
        }
        throw new CustomErrorClass();
      }).extension();
      
      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(statusWasCalled);
      assertTrue(jsonWasCalled);
      assertTrue(passedRequestAndResponse);
    },

    testExtensionIsChainable: function () {
      var constraint = joi.number().integer().description("The value of an a"),
      context;

      app.extend({
        "extension": function() {
          this.queryParam("a", {
            type: constraint
          });
        }
      });

      context = app.get('/foxx', function () {
        //nothing
      }).extension()
      .queryParam("b", {
        type: constraint
      });

      assertEqual(context.constraints.queryParams, {a: constraint, b: constraint});
      context = app.get('/foxx2', function () {
        //nothing
      })
      .queryParam("b", {
        type: constraint
      })
      .extension();

      assertEqual(context.constraints.queryParams, {a: constraint, b: constraint});
    },

    testExtensionUsingParams: function () {
      var constraint = joi.number().integer().description("The value of an a"),
      context;

      app.extend({
        "extension": function(name, type) {
          this.queryParam(name, {
            type: type
          });
        }
      });

      context = app.get('/foxx', function () {
        //nothing
      }).extension("a", constraint);

      assertEqual(context.constraints.queryParams, {a: constraint});
    }

  };
}

jsunity.run(CreateFoxxControllerSpec);
jsunity.run(SetRoutesFoxxControllerSpec);
jsunity.run(ControllerInjectionSpec);
jsunity.run(ExtendFoxxControllerSpec);
//jsunity.run(LegacyDocumentationAndConstraintsSpec);
jsunity.run(DocumentationAndConstraintsSpec);
jsunity.run(AddMiddlewareFoxxControllerSpec);
jsunity.run(CommentDrivenDocumentationSpec);
jsunity.run(SetupAuthorization);
jsunity.run(SetupSessions);
jsunity.run(FoxxControllerWithRootElement);

return jsunity.done();
