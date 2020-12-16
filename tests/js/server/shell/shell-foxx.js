/*jshint globalstrict:false, strict:false, unused:false */
/*global assertEqual, assertTrue, assertFalse, assertUndefined */

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

var stub,
  allow,
  FunctionStub,
  mockConstructor,
  joi = require("joi"),
  transformRoute = require("@arangodb/foxx/legacy/routing").__test_transformControllerToRoute,
  _ = require("lodash");

require("@arangodb/test-helper").waitForFoxxInitialized();

// Sorry for Yak Shaving. But I can't take it anymore.

// x = stub();

stub = function () {
  'use strict';
  return function() {};
};

// allow(x)
//   .toReceive("functionName")
//   .andReturn({ x: 1 })

FunctionStub = function(obj) {
  'use strict';
  this.obj = obj;
};

Object.assign(FunctionStub.prototype, {
  toReceive: function (functionName) {
    'use strict';
    this.functionName = functionName;
    this.buildFunctionStub();
    return this;
  },

  andReturn: function (returnValue) {
    'use strict';
    this.returnValue = returnValue;
    this.buildFunctionStub();
    return this;
  },

  buildFunctionStub: function () {
    'use strict';
    var returnValue = this.returnValue;

    this.obj[this.functionName] = function () {
      return returnValue;
    };
  }
});

allow = function(obj) {
  'use strict';
  return (new FunctionStub(obj));
};

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
mockConstructor = function () {
  'use strict';
  var expectedArguments = arguments,
    satisfied = false,
    MockConstructor = function () {
    if (this.constructor === MockConstructor) {
      // Was called as a constructor
      satisfied = _.isEqual(arguments, expectedArguments);
    }
  };

  MockConstructor.assertIsSatisfied = function () {
    assertTrue(satisfied);
  };

  return MockConstructor;
};


var jsunity = require("jsunity"),
  FoxxController = require("@arangodb/foxx/legacy").Controller,
  fakeContext,
  fakeContextWithRootElement;

fakeContext = {
  prefix: "",
  foxxes: [],
  manifest: {
    rootElement: false
  },
  collectionName() {}
};

fakeContextWithRootElement = {
  prefix: "",
  foxxes: [],
  manifest: {
    rootElement: true
  },
  collectionName() {}
};

function CreateFoxxControllerSpec () {
  'use strict';
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
  'use strict';
  var app;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
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
    },

    testAddALoginRoute: function () {
      var myFunc = function () {},
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
      var myFunc = function () {},
        error;

      try {
        app.login('/simple/route', myFunc);
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Setup authentication first"));
    },

    testAddALogoutRoute: function () {
      var myFunc = function () {},
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
      var myFunc = function () {},
        error;

      try {
        app.logout('/simple/route', myFunc);
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Setup authentication first"));
    },

    testAddADestroySessionRoute: function () {
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.activateSessions({
        sessionStorage: 'sessions',
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
      var myFunc = function () {},
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
  'use strict';
  return {
    testInjectFactoryFunction: function () {
      var app = new FoxxController(fakeContext),
        req = {},
        res = {},
        timesCalled = 0;

      app.addInjector({thing: function() {timesCalled++;}});

      app.get('/foxx', function () {});

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

      app.get('/foxx', function () {});

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

      app.get('/foxx', function () {});

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
  'use strict';
  var app, routes, models, Model;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
      routes = app.routingInfo.routes;
      models = app.models;
      Model = require('@arangodb/foxx/legacy').Model;
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
        jsonSchema = { required: [], properties: {} };

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
        jsonSchema = { required: [], properties: {} };

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
        jsonSchema = { required: [], properties: {} };

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
      ModelPrototype.toJSONSchema = function () { return { }; };

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

    testSetParamForJoiBodyParamWithAllowUnknownTrue: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = {x: 1, y: 2},
        schema = {x: joi.number().integer().required()},
        called = false;

      schema = joi.object().keys(schema).options({allowUnknown: true});

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = _.isEqual(providedReq.parameters[paramName], {x: 1, y: 2});
      }).bodyParam(paramName, {
        description: description,
        type: schema
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertTrue(called);
    },

    testSetParamForJoiBodyParamWithAllowUnknownFalse: function () {
      var req = { parameters: {} },
        res = {},
        paramName = 'flurb',
        description = stub(),
        requestBody = {x: 1, y: 2},
        schema = {x: joi.number().integer().required()},
        called = false;

      schema = joi.object().keys(schema).options({allowUnknown: false});

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function (providedReq) {
        called = true;
      }).bodyParam(paramName, {
        description: description,
        type: schema
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertFalse(called);
      assertEqual(res.responseCode, 422);
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
        called = false;

      allow(req)
        .toReceive("body")
        .andReturn(requestBody);

      app.get('/foxx', function () {
        called = true;
      }).bodyParam(paramName, {
        description: description,
        type: schema
      });

      var callback = transformRoute(routes[0].action);
      callback(req, res);

      assertFalse(called);
      assertEqual(res.responseCode, 422);
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
      var CustomErrorClass = function () {};

      app.get('/foxx', function () {
        //nothing
      }).errorResponse(CustomErrorClass, 400, "I don't understand a word you're saying");

      assertEqual(routes.length, 1);
      assertEqual(routes[0].docs.errorResponses.length, 1);
      assertEqual(routes[0].docs.errorResponses[0].code, 400);
      assertEqual(routes[0].docs.errorResponses[0].reason, "I don't understand a word you're saying");
    },

    testCatchesDefinedError: function () {
      var CustomErrorClass = function () {},
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
        CustomErrorClass = function () {};

      res = {
        status: function () {},
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
      var CustomErrorClass = function () {};

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
  'use strict';
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
  'use strict';
  var app, routingInfo, noop;

  return {
    setUp: function () {
      app = new FoxxController(fakeContext);
      routingInfo = app.routingInfo;
      noop = function () {};
    },

    testSettingTheSummary: function () {
      app.get('/simple/route', noop)
      .summary('Get all the foxes');

      assertEqual(routingInfo.routes[0].docs.summary, 'Get all the foxes');
    },

    testSettingTheNotes: function () {
      app.get('/simple/route', noop)
      .notes('A function to get all foxes from the database\nin a good way.');

      assertEqual(routingInfo.routes[0].docs.notes, "A function to get all foxes from the database\nin a good way.");
    }
  };
}

function SetupAuthorization () {
  'use strict';
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

      assertUndefined(err && err.stack);
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
  'use strict';
  var app;

  return {
    testWorksWithUnsignedCookies: function () {
      var err;

      app = new FoxxController(fakeContext);

      try {
        app.activateSessions({
          sessionStorage: 'sessions',
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
          sessionStorage: 'sessions',
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
          sessionStorage: 'sessions',
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
          sessionStorage: 'sessions',
          header: 'X-Session-Id'
        });
      } catch (e) {
        err = e;
      }

      assertUndefined(err);
    }
  };
}

function FoxxControllerWithRootElement () {
  'use strict';
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
  'use strict';
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
      var CustomErrorClass = function () {};

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
      var CustomErrorClass = function () {},
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
//jsunity.run(LegacyDocumentationAndConstraintsSpec);
jsunity.run(DocumentationAndConstraintsSpec);
jsunity.run(AddMiddlewareFoxxControllerSpec);
jsunity.run(CommentDrivenDocumentationSpec);
jsunity.run(SetupAuthorization);
jsunity.run(SetupSessions);
jsunity.run(FoxxControllerWithRootElement);

return jsunity.done();
