////////////////////////////////////////////////////////////////////////////////
/// @brief tests for routing
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");
var internal = require("internal");
var jsunity = require("jsunity");

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief single patterns routing
////////////////////////////////////////////////////////////////////////////////

function routingSuiteSingle () {
  var errors = internal.errors;
  var cn = "_routing";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { isSystem: true });

      collection.save({
        url: { match: "/hello/world" },
        content: "c1"
      });

      collection.save({
        url: "/world/hello",
        content: "c2"
      });

      collection.save({
        url: "/prefix/hello/*",
        content: "c3"
      });

      collection.save({
        url: { match: "/param/:hello/world", constraint: { hello: "[0-9]+" } },
        content: "c4"
      });

      collection.save({
        url: { match: "/opt/:hello?", constraint: { hello: "[0-9]+" }, methods: [ 'get' ] },
        content: "c5"
      });

      collection.save({
        url: "/json",
        content: { contentType: "application/json", body: '{"text": "c6"}' }
      });

      collection.save({
        url: "/p/h/*",
        content: "p1"
      });

      collection.save({
        url: "/p/h",
        content: "p2"
      });

      actions.reloadRouting();
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing
////////////////////////////////////////////////////////////////////////////////

    testSimpleRouting: function () {
      var r = actions.firstRouting('GET', "/hello/world");
      assertEqual('c1', r.route.route.content);

      assertEqual('/hello/world', r.route.path);
      assertEqual(undefined, r.prefix);
      assertEqual(undefined, r.suffix);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (sort-cut for match)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingShort: function () {
      var r = actions.firstRouting('GET', "/world/hello");
      assertEqual('c2', r.route.route.content);

      assertEqual('/world/hello', r.route.path);
      assertEqual(undefined, r.prefix);
      assertEqual(undefined, r.suffix);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (prefix)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingPrefix: function () {
      var r = actions.firstRouting('GET', "/prefix/hello/world");
      assertEqual('c3', r.route.route.content);

      assertEqual('/prefix/hello(/[^/]+)*', r.route.path);
      assertEqual('/prefix/hello', r.prefix);
      assertEqual(['world'], r.suffix);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (parameter)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingParameter: function () {
      var r = actions.firstRouting('GET', "/param/12345/world");
      assertEqual('c4', r.route.route.content);

      assertEqual('/param/[0-9]+/world', r.route.path);
      assertEqual(undefined, r.prefix);
      assertEqual(undefined, r.suffix);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);

      r = actions.firstRouting('GET', "/param/a12345/world");
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (optional)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingOptional: function () {
      var r = actions.firstRouting('GET', "/opt/12345");
      assertEqual('c5', r.route.route.content);

      assertEqual('/opt(/[0-9]+)?', r.route.path);
      assertEqual(undefined, r.prefix);
      assertEqual(undefined, r.suffix);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);

      r = actions.firstRouting('GET', "/opt");
      assertEqual('c5', r.route.route.content);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (optional)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingMethod: function () {
      var r = actions.firstRouting('HEAD', "/opt/12345");
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (prefix vs non-prefix)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingNonPrefix: function () {
      var r = actions.firstRouting('GET', "/p/h");
      assertEqual('p2', r.route.route.content);

      assertEqual('/p/h', r.route.path);
      assertEqual(undefined, r.prefix);
      assertEqual(undefined, r.suffix);

      r = actions.nextRouting(r);

      assertEqual('/p/h(/[^/]+)*', r.route.path);
      assertEqual('/p/h', r.prefix);
      assertEqual([], r.suffix);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: content string
////////////////////////////////////////////////////////////////////////////////

    testContentString: function () {
      var r = actions.firstRouting('GET', "/opt/12345");

      req = {};
      res = {};

      r.route.callback.controller(req, res);

      assertEqual(actions.HTTP_OK, res.responseCode);
      assertEqual("text/plain", res.contentType);
      assertEqual("c5", res.body);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: content json
////////////////////////////////////////////////////////////////////////////////

    testContentJson: function () {
      var r = actions.firstRouting('GET', "/json");

      req = {};
      res = {};

      r.route.callback.controller(req, res);

      assertEqual(actions.HTTP_OK, res.responseCode);
      assertEqual("application/json", res.contentType);
      assertEqual('{"text": "c6"}', res.body);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief bundle without prefix
////////////////////////////////////////////////////////////////////////////////

function routingSuiteBundle () {
  var errors = internal.errors;
  var cn = "_routing";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { isSystem: true });

      collection.save({
        middleware: [
          { url: { match: "/*" }, content: "m1" },
          { url: { match: "/hello/*" }, content: "m2" },
          { url: { match: "/hello/world" }, content: "m3" },
          { url: { match: "/:name/world" }, content: "m4" }
        ],

        routes: [
          { url: { match: "/*" }, content: "c1" },
          { url: { match: "/hello/*" }, content: "c2" },
          { url: { match: "/hello/world" }, content: "c3" },
          { url: { match: "/hello/:name|:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "c4" },
          { url: { match: "/hello/:name/:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "c5" },
          { url: { match: "/:name/world" }, content: "c6" },
          { url: { match: "/hello" }, content: "c7" }
        ]
      });

      actions.reloadRouting();
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief test: routing cache
////////////////////////////////////////////////////////////////////////////////

    testRoutingCache: function () {
      var cache = actions.routingCache();

      assertEqual(3, cache.routes.GET.exact.hello.parameters.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing
////////////////////////////////////////////////////////////////////////////////

    testSimpleRouting: function () {
      var r = actions.firstRouting('GET', "/hello/world");
      assertEqual('m1', r.route.route.content);
      assertEqual('(/[^/]+)*', r.route.path);

      // middleware: unspecific to specific
      r = actions.nextRouting(r);
      assertEqual('m4', r.route.route.content);
      assertEqual('/[^/]+/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m2', r.route.route.content);
      assertEqual('/hello(/[^/]+)*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m3', r.route.route.content);
      assertEqual('/hello/world', r.route.path);

      // routing: specific to unspecific
      r = actions.nextRouting(r);
      assertEqual('c3', r.route.route.content);
      assertEqual('/hello/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c4', r.route.route.content);
      assertEqual(1, r.route.urlParameters.name);
      assertEqual('/hello/[a-z]+', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c2', r.route.route.content);
      assertEqual('/hello(/[^/]+)*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c6', r.route.route.content);
      assertEqual('/[^/]+/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c1', r.route.route.content);
      assertEqual('(/[^/]+)*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief bundle with prefix
////////////////////////////////////////////////////////////////////////////////

function routingSuitePrefix () {
  var errors = internal.errors;
  var cn = "_routing";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { isSystem: true });

      collection.save({
        urlPrefix: "/test",

        middleware: [
          { url: { match: "/*" }, content: "m1" },
          { url: { match: "/hello/*" }, content: "m2" },
          { url: { match: "/hello/world" }, content: "m3" },
          { url: { match: "/:name/world" }, content: "m4" }
        ],

        routes: [
          { url: { match: "/*" }, content: "c1" },
          { url: { match: "/hello/*" }, content: "c2" },
          { url: { match: "/hello/world" }, content: "c3" },
          { url: { match: "/hello/:name|:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "c4" },
          { url: { match: "/hello/:name/:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "c5" },
          { url: { match: "/:name/world" }, content: "c6" },
          { url: { match: "/hello" }, content: "c7" }
        ]
      });

      actions.reloadRouting();
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief test: routing cache
////////////////////////////////////////////////////////////////////////////////

    testRoutingCache: function () {
      var cache = actions.routingCache();

      assertEqual(3, cache.routes.GET.exact.test.exact.hello.parameters.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing
////////////////////////////////////////////////////////////////////////////////

    testSimpleRouting: function () {
      var r = actions.firstRouting('GET', "/test/hello/world");
      assertEqual('m1', r.route.route.content);
      assertEqual('/test(/[^/]+)*', r.route.path);

      // middleware: unspecific to specific
      r = actions.nextRouting(r);
      assertEqual('m4', r.route.route.content);
      assertEqual('/test/[^/]+/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m2', r.route.route.content);
      assertEqual('/test/hello(/[^/]+)*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m3', r.route.route.content);
      assertEqual('/test/hello/world', r.route.path);

      // routing: specific to unspecific
      r = actions.nextRouting(r);
      assertEqual('c3', r.route.route.content);
      assertEqual('/test/hello/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c4', r.route.route.content);
      assertEqual(2, r.route.urlParameters.name);
      assertEqual('/test/hello/[a-z]+', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c2', r.route.route.content);
      assertEqual('/test/hello(/[^/]+)*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c6', r.route.route.content);
      assertEqual('/test/[^/]+/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c1', r.route.route.content);
      assertEqual('/test(/[^/]+)*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(routingSuiteSingle);
jsunity.run(routingSuiteBundle);
jsunity.run(routingSuitePrefix);

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
