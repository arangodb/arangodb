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

function AnySuite () {
  'use strict';
  var cn = "example";
  var c;

  var statsExpected = function (list, N) {
    // list is a list of numbers, we assume that we draw N times one number
    // from list with a uniform distribution, we compute the expected value
    // of the average and the variance, as well as the variance
    // of these two:

    // First the expected value and the standard deviation of the uniform
    // distribution:
    var E = 0;
    var n = list.length;
    var i;
    for (i = 0; i < n; i++) {
      E += list[i];
    }
    E /= n;
    var V = 0;
    for (i = 0; i < n; i++) {
      V += Math.pow(list[i] - E, 2);
    }
    V = V / n;

    // Now we apply the central limit theorem to the random variable
    // Y = (X - E)^2, first compute its expected value and standard 
    // deviation:
    var EY = V;
    var VY = 0;
    for (i = 0; i < n; i++) {
      VY += Math.pow(Math.pow(list[i] - E, 2) - EY, 2);
    }
    VY = VY / n;
    // Now EY is V and sY is its variance, by the central limit theorem
    // taking the average of a sample of size N of Y values will be close 
    // to the normal distribution with expected value EY and variance
    // VY / N
    
    return { average: E, variance: V,
             averageStddev: Math.sqrt(V) / Math.sqrt(N),
             varianceStddev: Math.sqrt(VY) / Math.sqrt(N) };
  };
 
  var statsFound = function (dist) {
    var v;
    var sum = 0;
    var count = 0;

    for (v in dist) {
      if (dist.hasOwnProperty(v)) {
        sum += dist[v] * Number(v);
        count += dist[v];
      }
    }

    var avg = sum / count;
    var sum2 = 0;

    for (v in dist) {
      if (dist.hasOwnProperty(v)) {
        var d = Number(v) - avg;
        sum2 += d * d * dist[v];
      }
    }

    return { average: avg, count: count, variance : sum2 / (count-1) };
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
      var i, n, l;

      n = 100;

      l = [];
      for (i = 0; i < n; ++i) {
        c.save({ value: i });
        l.push(i);
      }

      var dist = getDistribution(n * 100, function () {
        return parseInt(Math.random() * 100, 10);
      });

      var statsExp = statsExpected(l, n * 100);
      var stats = statsFound(dist);
      assertEqual(stats.count, n * 100);
      assertTrue(Math.abs(stats.average - statsExp.average) 
                 < statsExp.averageStddev * 3);
      assertTrue(Math.abs(stats.variance - statsExp.variance)
                 < statsExp.varianceStddev * 3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), just one document
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionOne : function () {
      c.save({ value: 1 });

      var dist = getDistribution(100, function () {
        return c.any().value;
      });
      
      var statsExp = statsExpected([1], 100);
      var stats = statsFound(dist);
      assertEqual(stats.count, 100);
      assertTrue(Math.abs(stats.average - statsExp.average) < 0.000001);
      assertTrue(Math.abs(stats.variance - statsExp.variance) < 0.000001);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), few picks
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionFew1 : function () {
      var i, n, l;

      n = 3;

      l = [];
      for (i = 0; i < n; ++i) {
        c.save({ value: i });
        l.push(i);
      }

      var dist = getDistribution(n * 200, function () {
        return c.any().value;
      });
      
      var statsExp = statsExpected(l, n * 200);
      var stats = statsFound(dist);
      assertEqual(stats.count, n * 200);
      assertTrue(Math.abs(stats.average - statsExp.average) 
                 < statsExp.averageStddev * 3);
      assertTrue(Math.abs(stats.variance - statsExp.variance)
                 < statsExp.varianceStddev * 5);
      // Note: we take 5 standard deviations here, because 3 lead to
      // many false positives. Probably our estimate for the stddev or
      // the quality of the rng is not actually good enough.
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), few picks
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionFew2 : function () {
      var i, n, l;

      n = 10;

      l = [];
      for (i = 0; i < n; ++i) {
        c.save({ value: i });
        l.push(i);
      }

      var dist = getDistribution(n * 100, function () {
        return c.any().value;
      });
      
      var statsExp = statsExpected(l, n * 100);
      var stats = statsFound(dist);
      assertEqual(stats.count, n * 100);
      assertTrue(Math.abs(stats.average - statsExp.average) 
                 < statsExp.averageStddev * 3);
      assertTrue(Math.abs(stats.variance - statsExp.variance)
                 < statsExp.varianceStddev * 3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), more picks
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionMore : function () {
      var i, n, l;

      n = 100;

      l = [];
      for (i = 0; i < n; ++i) {
        c.save({ value: i });
        l.push(i);
      }
      
      var dist = getDistribution(n * 100, function () {
        return c.any().value;
      });

      
      var statsExp = statsExpected(l, n * 100);
      var stats = statsFound(dist);
      assertEqual(stats.count, n * 100);
      assertTrue(Math.abs(stats.average - statsExp.average) 
                 < statsExp.averageStddev * 3);
      assertTrue(Math.abs(stats.variance - statsExp.variance)
                 < statsExp.varianceStddev * 3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), with many documents deleted
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionHalf : function () {
      var i, n, l;

      n = 100;

      for (i = 0; i < n; ++i) {
        c.save({ value: i });
      }

      // remove 50 % of entries
      var d = Math.round(n * 0.5);
      for (i = 0; i < d; ++i) {
        c.remove(c.any());
      }

      l = db._query(`FOR d IN ${cn} RETURN d.value`).toArray();

      var dist = getDistribution(n * 50, function () {
        return c.any().value;
      });

      var statsExp = statsExpected(l, n * 50);
      var stats = statsFound(dist);
      assertEqual(stats.count, n * 50);
      assertTrue(Math.abs(stats.average - statsExp.average) 
                 < statsExp.averageStddev * 3);
      assertTrue(Math.abs(stats.variance - statsExp.variance)
                 < statsExp.varianceStddev * 3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check entropy of any(), with most documents deleted
////////////////////////////////////////////////////////////////////////////////

    testCheckEntropyCollectionSparse : function () {
      var i, n, l;

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

      l = db._query(`FOR d IN ${cn} RETURN d.value`).toArray();

      var statsExp = statsExpected(l, n * 5);
      var stats = statsFound(dist);
      assertEqual(stats.count, n * 5);
      assertTrue(Math.abs(stats.average - statsExp.average) 
                 < statsExp.averageStddev * 3);
      assertTrue(Math.abs(stats.variance - statsExp.variance)
                 < statsExp.varianceStddev * 3);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AnySuite);

return jsunity.done();

