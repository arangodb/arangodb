/*jshint globalstrict:false, strict:false, globalstrict: true */
/*global assertEqual */

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

'use strict';
var actions = require("@arangodb/actions");
var jsunity = require("jsunity");

var flattenRoutingTree = actions.flattenRoutingTree;
var buildRoutingTree = actions.buildRoutingTree;


////////////////////////////////////////////////////////////////////////////////
/// @brief single patterns routing
////////////////////////////////////////////////////////////////////////////////

function routingSuiteSingle () {
  var routing;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var routes = [{
        routes: [
          {
            url: { match: "/hello/world" },
            content: "c1"
          },

          {
            url: "/world/hello",
            content: "c2"
          },

          {
            url: "/prefix/hello/*",
            content: "c3"
          },

          {
            url: { match: "/param/:hello/world", constraint: { hello: "[0-9]+" } },
            content: "c4"
          },

          {
            url: { match: "/opt/:hello?", constraint: { hello: "[0-9]+" }, methods: [ 'get' ] },
            content: "c5"
          },

          {
            url: "/json",
            content: { contentType: "application/json", body: '{"text": "c6"}' }
          },

          {
            url: "/p/h/*",
            content: "p1"
          },

          {
            url: "/p/h",
            content: "p2"
          }
        ]
      }];

      routing = flattenRoutingTree(buildRoutingTree(routes));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing
////////////////////////////////////////////////////////////////////////////////

    testSimpleRouting: function () {
      var r = actions.firstRouting('GET', "/hello/world", routing);

      assertEqual('c1', r.route.route.content);
      assertEqual('/hello/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (sort-cut for match)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingShort: function () {
      var r = actions.firstRouting('GET', "/world/hello", routing);

      assertEqual('c2', r.route.route.content);
      assertEqual('/world/hello', r.route.path);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (prefix)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingPrefix: function () {
      var r = actions.firstRouting('GET', "/prefix/hello/world", routing);

      assertEqual('c3', r.route.route.content);
      assertEqual('/prefix/hello/*', r.route.path);
      assertEqual('/prefix/hello', r.prefix);
      assertEqual(['world'], r.suffix);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (parameter)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingParameter: function () {
      var r = actions.firstRouting('GET', "/param/12345/world", routing);

      assertEqual('c4', r.route.route.content);
      assertEqual('/param/:hello/world', r.route.path);
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
      var r = actions.firstRouting('GET', "/opt/12345", routing);

      assertEqual('c5', r.route.route.content);
      assertEqual('/opt/:hello?', r.route.path);
      assertEqual(undefined, r.prefix);
      assertEqual(undefined, r.suffix);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (optional)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingOptional2: function () {
      var r = actions.firstRouting('GET', "/opt", routing);
      assertEqual('c5', r.route.route.content);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (optional)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingMethod: function () {
      var r = actions.firstRouting('HEAD', "/opt/12345", routing);
      assertEqual(undefined, r.route);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing (prefix vs non-prefix)
////////////////////////////////////////////////////////////////////////////////

    testSimpleRoutingNonPrefix: function () {
      var r = actions.firstRouting('GET', "/p/h", routing);

      assertEqual('p2', r.route.route.content);
      assertEqual('/p/h', r.route.path);
      assertEqual(undefined, r.prefix);
      assertEqual(undefined, r.suffix);

      r = actions.nextRouting(r);

      assertEqual('/p/h/*', r.route.path);
      assertEqual('/p/h', r.prefix);
      assertEqual([], r.suffix);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: content string
////////////////////////////////////////////////////////////////////////////////

    testContentString: function () {
      var r = actions.firstRouting('GET', "/opt/12345", routing);

      var req = {};
      var res = {};

      r.route.callback.controller(req, res);

      assertEqual(actions.HTTP_OK, res.responseCode);
      assertEqual("text/plain", res.contentType);
      assertEqual("c5", res.body);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: content json
////////////////////////////////////////////////////////////////////////////////

    testContentJson: function () {
      var r = actions.firstRouting('GET', "/json", routing);

      var req = {};
      var res = {};

      r.route.callback.controller(req, res);

      assertEqual(actions.HTTP_OK, res.responseCode);
      assertEqual("application/json", res.contentType);
      assertEqual('{"text": "c6"}', res.body);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief bundle without prefix
////////////////////////////////////////////////////////////////////////////////

function routingSuiteBundle () {
  var routing;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var routes = [{
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
          { url: { match: "/hello/:name/:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "c5" },
          { url: { match: "/:name/world" }, content: "c6" },
          { url: { match: "/hello" }, content: "c7" }
        ]
      }];

      routing = flattenRoutingTree(buildRoutingTree(routes));
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing
////////////////////////////////////////////////////////////////////////////////

    testBundleSimpleRouting: function () {
      var r = actions.firstRouting('GET', "/hello/world", routing);

      assertEqual('m1', r.route.route.content);
      assertEqual('/*', r.route.path);

      // middleware: unspecific to specific
      r = actions.nextRouting(r);
      assertEqual('m4', r.route.route.content);
      assertEqual('/:name/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m2', r.route.route.content);
      assertEqual('/hello/*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m3', r.route.route.content);
      assertEqual('/hello/world', r.route.path);

      // routing: specific to unspecific
      r = actions.nextRouting(r);
      assertEqual('c3', r.route.route.content);
      assertEqual('/hello/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c2', r.route.route.content);
      assertEqual('/hello/*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c6', r.route.route.content);
      assertEqual('/:name/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c1', r.route.route.content);
      assertEqual('/*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief bundle with prefix
////////////////////////////////////////////////////////////////////////////////

function routingSuitePrefix () {
  var routing;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var routes = [{
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
          { url: { match: "/hello/:name/:id", constraint: { name: "/[a-z]+/", id: "/[0-9]+/" } }, content: "c5" },
          { url: { match: "/:name/world" }, content: "c6" },
          { url: { match: "/hello" }, content: "c7" }
        ]
      }];

      routing = flattenRoutingTree(buildRoutingTree(routes));
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief test: simple routing
////////////////////////////////////////////////////////////////////////////////

    testPrefixSimpleRouting: function () {
      var r = actions.firstRouting('GET', "/test/hello/world", routing);
      assertEqual('m1', r.route.route.content);
      assertEqual('/test/*', r.route.path);

      // middleware: unspecific to specific
      r = actions.nextRouting(r);
      assertEqual('m4', r.route.route.content);
      assertEqual('/test/:name/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m2', r.route.route.content);
      assertEqual('/test/hello/*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('m3', r.route.route.content);
      assertEqual('/test/hello/world', r.route.path);

      // routing: specific to unspecific
      r = actions.nextRouting(r);
      assertEqual('c3', r.route.route.content);
      assertEqual('/test/hello/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c2', r.route.route.content);
      assertEqual('/test/hello/*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c6', r.route.route.content);
      assertEqual('/test/:name/world', r.route.path);

      r = actions.nextRouting(r);
      assertEqual('c1', r.route.route.content);
      assertEqual('/test/*', r.route.path);

      r = actions.nextRouting(r);
      assertEqual(undefined, r.route);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(routingSuiteSingle);
jsunity.run(routingSuiteBundle);
jsunity.run(routingSuitePrefix);

return jsunity.done();


