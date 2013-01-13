var jsunity = require("jsunity"),
  internal = require("internal"),
  console = require("console"),
  Frank = require("org/arangodb/frank").Frank;

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
      var myFunc = function () {},
        routes = app.routingInfo.routes;

      app.get('/simple/route', myFunc);
      assertEqual(routes.length, 1);
      assertEqual(routes[0].handler, myFunc);
    }

    //app.get('/', function() {
      // erb "index" <- from templates collection?
      // not really erb... underscore_template is much too long
    //});
  };
}

jsunity.run(CreateFrankSpec);
jsunity.run(SetRoutesFrankSpec);

return jsunity.done();
