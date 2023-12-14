/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the random document selector 
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var db = arangodb.db;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AnySuite() {
  'use strict';
  var cn = "example";
  var c;

  var getDistribution = function (n, rng) {
    var dist = {};
    var i;

    for (i = 0; i < n; ++i) {
      var pick = rng();

      if (dist.hasOwnProperty(pick)) {
        dist[pick]++;
      }
      else {
        dist[pick] = 1;
      }
    }

    return dist;
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check entropy of any(), few picks
    ////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionFew1: function () {
      var i, n, l;

      n = 3;

      l = [];
      for (i = 0; i < n; ++i) {
        l.push({ value: i });
      }
      c.save(l);

      var dist = getDistribution(n * 200, function () {
        return c.any().value;
      });

      // make sure we hit everything
      assertEqual(Object.keys(dist).length, 3);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check entropy of any(), few picks
    ////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionFew2: function () {
      var i, n, l;

      n = 10;

      l = [];
      for (i = 0; i < n; ++i) {
        l.push({ value: i });
      }
      c.save(l);

      var dist = getDistribution(n * 100, function () {
        return c.any().value;
      });

      // make sure we hit everything
      assertEqual(Object.keys(dist).length, 10);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check entropy of any(), more picks
    ////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionMore: function () {
      var i, n, l;

      n = 500;

      l = [];
      for (i = 0; i < n; ++i) {
        l.push({ value: i });
      }
      c.save(l);

      var dist = getDistribution(n * 100, function () {
        return c.any().value;
      });


      // make sure we hit enough
      assertTrue(Object.keys(dist).length >= 20);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check entropy of any(), with many documents deleted
    ////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionHalf: function () {
      var i, n, l;

      n = 500;

      l = [];
      for (i = 0; i < n; ++i) {
        l.push({ value: i });
      }
      c.save(l);

      // remove 50 % of entries
      var d = Math.round(n * 0.5);
      for (i = 0; i < d; ++i) {
        c.remove(c.any());
      }

      l = db._query(`FOR d IN ${cn} RETURN d.value`).toArray();

      var dist = getDistribution(n * 50, function () {
        return c.any().value;
      });

      // make sure we hit enough
      assertTrue(Object.keys(dist).length >= 20);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief check entropy of any(), with most documents deleted
    ////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionSparse: function () {
      var i, n, l;

      n = 500;

      l = [];
      for (i = 0; i < n; ++i) {
        l.push({ value: i });
      }
      c.save(l);

      // remove 50 % of entries
      var d = Math.round(n * 0.95);
      for (i = 0; i < d; ++i) {
        c.remove(c.any());
      }

      var dist = getDistribution(n * 5, function () {
        return c.any().value;
      });

      // make sure we hit enough
      assertTrue(Object.keys(dist).length >= 10);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AnySuite);

return jsunity.done();

