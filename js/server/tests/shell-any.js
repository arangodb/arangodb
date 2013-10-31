////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
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

var arangodb = require("org/arangodb");
var internal = require("internal");
var db = arangodb.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                               any
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AnySuite () {
  var cn = "example";
  var c;

  var threshold = 15; // maximum tolerated stddev for distribution

  var stddev = function (dist) {
    var v;
    var sum = 0;
    var count = 0;

    for (v in dist) {
      if (dist.hasOwnProperty(v)) {
        sum += dist[v];
        count++;
      }
    }

    var avg = sum / count;
    var sum2 = 0;

    for (v in dist) {
      if (dist.hasOwnProperty(v)) {
        var d = dist[v] - avg;
        sum2 += d * d;
      }
    }

    return Math.sqrt(sum2 / count);
  };

  var getDistribution = function (n, rng) {
    var dist = { };
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

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of Math.random()
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyNative : function () {
      var i, n;

      n = 100;

      for (i = 0; i < n; ++i) {
        c.save({ value: i });
      }

      var dist = getDistribution(n * 100, function () {
        return parseInt(Math.random() * 100, 10);
      });

      assertTrue(stddev(dist) < threshold);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), just one document
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionOne : function () {
      c.save({ value: 1 });

      var dist = getDistribution(100, function () {
        return c.any().value;
      });
      
      assertTrue(stddev(dist) <= 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), few picks
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionFew1 : function () {
      var i, n;

      n = 3;

      for (i = 0; i < n; ++i) {
        c.save({ value: i });
      }

      var dist = getDistribution(n * 200, function () {
        return c.any().value;
      });
      
      assertTrue(stddev(dist) < threshold * 1.5);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), few picks
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionFew2 : function () {
      var i, n;

      n = 10;

      for (i = 0; i < n; ++i) {
        c.save({ value: i });
      }

      var dist = getDistribution(n * 100, function () {
        return c.any().value;
      });
      
      assertTrue(stddev(dist) < threshold);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), more picks
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionMore : function () {
      var i, n;

      n = 500;

      for (i = 0; i < n; ++i) {
        c.save({ value: i });
      }
      
      var dist = getDistribution(n * 100, function () {
        return c.any().value;
      });

      assertTrue(stddev(dist) < threshold);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), with many documents deleted
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionHalf : function () {
      var i, n;

      n = 500;

      for (i = 0; i < n; ++i) {
        c.save({ value: i });
      }

      // remove 50 % of entries
      var d = Math.round(n * 0.5);
      for (i = 0; i < d; ++i) {
        c.remove(c.any());
      }

      var dist = getDistribution(n * 50, function () {
        return c.any().value;
      });

      assertTrue(stddev(dist) < threshold);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), with most documents deleted
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionSparse : function () {
      var i, n;

      n = 500;

      for (i = 0; i < n; ++i) {
        c.save({ value: i });
      }

      // remove 50 % of entries
      var d = Math.round(n * 0.95);
      for (i = 0; i < d; ++i) {
        c.remove(c.any());
      }

      var dist = getDistribution(n * 5, function () {
        return c.any().value;
      });

      assertTrue(stddev(dist) < threshold);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AnySuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

