/*global require, fail, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the endpoints
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("org/arangodb");
var arango = arangodb.arango;
var db = arangodb.db;
var ERRORS = arangodb.errors;
var originalEndpoint = arango.getEndpoint().replace(/localhost/, '127.0.0.1');

// -----------------------------------------------------------------------------
// --SECTION--                                                   endpoints tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function EndpointsSuite () {
  var cleanupEndpoints = function () {
    db._useDatabase("_system");

    db._listEndpoints().forEach(function (e) {
      if (e.endpoint.replace(/localhost/, '127.0.0.1') !== originalEndpoint) {
        db._removeEndpoint(e.endpoint);
      }
    });
  };

  var filterDefault = function (endpoints) {
    var result = [ ];

    endpoints.forEach(function (e) {
      if (e.endpoint.replace(/localhost/, '127.0.0.1') !== originalEndpoint) {
        result.push(e);
      }
    });

    return result;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      cleanupEndpoints();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      cleanupEndpoints();

      var i;
      for (i = 0; i < 3; ++i) {
        try {
          db._dropDatabase("UnitTestsDatabase" + i);
        }
        catch (err) {
        }
      }

      arango.reconnect(originalEndpoint, "_system", "root", "");
      db._useDatabase("_system");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief add a few endpoints and check the list result
////////////////////////////////////////////////////////////////////////////////

    testListEndpoints : function () {
      assertEqual("_system", db._name());

      var i;
      var base = 30000 + parseInt(Math.random() * 10000, 10);

      // open a few endpoints
      var dbNames = [ ];
      for (i = 0; i < 3; ++i) {
        db._configureEndpoint("tcp://127.0.0.1:" + (base + i), dbNames);
        dbNames.push("db" + i);
      }

      var expected = [
        { endpoint: "tcp://127.0.0.1:" + base, databases: [ ] },
        { endpoint: "tcp://127.0.0.1:" + (base + 1), databases: [ "db0" ] },
        { endpoint: "tcp://127.0.0.1:" + (base + 2), databases: [ "db0", "db1" ] }
      ];

      var actual = filterDefault(db._listEndpoints());
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test configure
////////////////////////////////////////////////////////////////////////////////

    testConfigureEndpoints : function () {
      assertEqual("_system", db._name());

      var i;
      var base = 30000 + parseInt(Math.random() * 10000, 10);
      var ports = [ ];
      var iterations = 0;

      // open a few endpoints
      var dbNames = [ ];

      for (i = 0; i < 3; ++i) {
        // determine which port we can use
        while (! require("internal").testPort("tcp://127.0.0.1:" + base)) {
          ++base;
          if (base > 65535) {
            base = 2048;
          }

          if (++iterations > 100) {
            // prevent endless looping
            fail();
          }
        }
        ports[i] = base;

        db._configureEndpoint("tcp://127.0.0.1:" + ports[i], dbNames);
        dbNames.push("db" + i);
      }

      db._configureEndpoint("tcp://127.0.0.1:" + ports[2], [ ]);
      db._configureEndpoint("tcp://127.0.0.1:" + ports[1], [ "abc", "def" ]);
      db._configureEndpoint("tcp://127.0.0.1:" + ports[0], [ "_system" ]);

      var expected = [
        { endpoint: "tcp://127.0.0.1:" + ports[0], databases: [ "_system" ] },
        { endpoint: "tcp://127.0.0.1:" + ports[1], databases: [ "abc", "def" ] },
        { endpoint: "tcp://127.0.0.1:" + ports[2], databases: [ ] }
      ];

      var actual = filterDefault(db._listEndpoints());
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test configure
////////////////////////////////////////////////////////////////////////////////

    testConfigureInvalidEndpoints : function () {
      assertEqual("_system", db._name());

      var base = 30000 + parseInt(Math.random() * 10000, 10);

      try {
        // invalid type for 2nd argument
        db._configureEndpoint("tcp://127.0.0.1:" + base, { });
        fail();
      }
      catch (err1) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err1.errorNum);
      }

      try {
        // invalid type for 2nd argument
        db._configureEndpoint("tcp://127.0.0.1:" + base, "_system");
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err2.errorNum);
      }

      try {
        // invalid type for 1st argument
        db._configureEndpoint("foo", "_system");
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err3.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveEndpoints : function () {
      assertEqual("_system", db._name());

      var i;
      var base = 30000 + parseInt(Math.random() * 10000, 10);

      // open a few endpoints
      var dbNames = [ ];
      for (i = 0; i < 3; ++i) {
        db._configureEndpoint("tcp://127.0.0.1:" + (base + i), dbNames);
        dbNames.push("db" + i);
      }

      // remove one
      assertTrue(db._removeEndpoint("tcp://127.0.0.1:" + base));

      var expected = [
        { endpoint: "tcp://127.0.0.1:" + (base + 1), databases: [ "db0" ] },
        { endpoint: "tcp://127.0.0.1:" + (base + 2), databases: [ "db0", "db1" ] }
      ];

      var actual = filterDefault(db._listEndpoints());
      assertEqual(expected, actual);

      // remove next
      assertTrue(db._removeEndpoint("tcp://127.0.0.1:" + (base + 2)));

      expected = [
        { endpoint: "tcp://127.0.0.1:" + (base + 1), databases: [ "db0" ] }
      ];

      actual = filterDefault(db._listEndpoints());
      assertEqual(expected, actual);

      // remove last
      assertTrue(db._removeEndpoint("tcp://127.0.0.1:" + (base + 1)));

      expected = [
      ];

      actual = filterDefault(db._listEndpoints());
      assertEqual(expected, actual);

      try {
        // remove non-existing
        db._removeEndpoint("tcp://127.0.0.1:" + base);
        fail();
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test database access
////////////////////////////////////////////////////////////////////////////////

    testDatabaseAccess : function () {
      // create some databases
      db._createDatabase("UnitTestsDatabase0");
      db._createDatabase("UnitTestsDatabase1");
      db._createDatabase("UnitTestsDatabase2");

      var base = 30000 + parseInt(Math.random() * 10000, 10);

      // create a few endpoints
      db._configureEndpoint("tcp://127.0.0.1:" + (base), [ ]);
      db._configureEndpoint("tcp://127.0.0.1:" + (base + 1), [ "UnitTestsDatabase0" ]);
      db._configureEndpoint("tcp://127.0.0.1:" + (base + 2), [ "UnitTestsDatabase1" ]);
      db._configureEndpoint("tcp://127.0.0.1:" + (base + 3), [ "UnitTestsDatabase0", "UnitTestsDatabase1" ]);
      db._configureEndpoint("tcp://127.0.0.1:" + (base + 4), [ "UnitTestsDatabase1", "_system" ]);

      // and now connect to the endpoints
      var test = function (endpoint, endpoints) {
        endpoints.forEach(function (e) {
          if (e[1] === null) {
            try {
              arango.reconnect(endpoint, e[0], "root", "");
              fail();
            }
            catch (err) {
            }
          }
          else {
            arango.reconnect(endpoint, e[0], "root", "");
            db._queryProperties(true);
            //require("internal").print("EXPECTING: ", e[1], "ACTUAL: ", db._name());
            assertEqual(e[1], db._name());
          }
        });
      };

      var endpoints;

      endpoints = [
        [ "", "_system" ],
        [ "_system", "_system" ],
        [ "UnitTestsDatabase0", "UnitTestsDatabase0" ],
        [ "UnitTestsDatabase1", "UnitTestsDatabase1" ]
      ];

      test("tcp://127.0.0.1:" + (base), endpoints);

      endpoints = [
        [ "", "UnitTestsDatabase0" ],
        [ "_system", null ],
        [ "UnitTestsDatabase0", "UnitTestsDatabase0" ],
        [ "UnitTestsDatabase1", null ]
      ];

      test("tcp://127.0.0.1:" + (base + 1), endpoints);


      endpoints = [
        [ "", "UnitTestsDatabase1" ],
        [ "_system", null ],
        [ "UnitTestsDatabase0", null ],
        [ "UnitTestsDatabase1", "UnitTestsDatabase1" ]
      ];

      test("tcp://127.0.0.1:" + (base + 2), endpoints);


      endpoints = [
        [ "", "UnitTestsDatabase0" ],
        [ "_system", null ],
        [ "UnitTestsDatabase0", "UnitTestsDatabase0" ],
        [ "UnitTestsDatabase1", "UnitTestsDatabase1" ]
      ];

      test("tcp://127.0.0.1:" + (base + 3), endpoints);


      endpoints = [
        [ "", "UnitTestsDatabase1" ],
        [ "_system", "_system" ],
        [ "UnitTestsDatabase0", null ],
        [ "UnitTestsDatabase1", "UnitTestsDatabase1" ]
      ];

      test("tcp://127.0.0.1:" + (base + 4), endpoints);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-system
////////////////////////////////////////////////////////////////////////////////

    testEndpointsNonSystem : function () {
      assertEqual("_system", db._name());

      try {
        db._dropDatabase("UnitTestsDatabase0");
      }
      catch (err1) {
      }

      db._createDatabase("UnitTestsDatabase0");
      db._useDatabase("UnitTestsDatabase0");

      // _listEndpoints is forbidden
      try {
        db._listEndpoints();
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err2.errorNum);
      }

      // _configureEndpoint is forbidden
      try {
        var base = 30000 + parseInt(Math.random() * 10000, 10);
        db._configureEndpoint("tcp://127.0.0.1:" + base, [ ]);
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err3.errorNum);
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(EndpointsSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

