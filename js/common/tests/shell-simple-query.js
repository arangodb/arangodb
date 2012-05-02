/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require,
    db,
    assertEqual, assertTrue,
    print,
    PRINT_OBJECT,
    console,
    AvocadoCollection, AvocadoEdgesCollection */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the graph class
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var SQB = require("simple-query-basics");

// -----------------------------------------------------------------------------
// --SECTION--                                            basic skips and limits
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: skip and limit with a collection
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryArraySkipLimitSuite () {
  var numbers = null;
  var query = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      numbers = [0,1,2,3,4,5,6,7,8,9];
      query = new SQB.SimpleQueryArray(numbers);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: skip
////////////////////////////////////////////////////////////////////////////////

    testSkip : function () {
      var n = query.clone().skip(0).toArray();

      assertEqual(n, numbers);

      n = query.clone().skip(1).toArray();

      assertEqual(n, numbers.slice(1,10));

      n = query.clone().skip(2).toArray();

      assertEqual(n, numbers.slice(2,10));

      n = query.clone().skip(1).skip(1).toArray();

      assertEqual(n, numbers.slice(2,10));

      n = query.clone().skip(10).toArray();
      
      assertEqual(n, []);

      n = query.clone().skip(11).toArray();

      assertEqual(n, []);

      n = query.clone().skip(9).toArray();

      assertEqual(n, [numbers[9]]);

      n = query.clone().skip(9).skip(1).toArray();

      assertEqual(n, []);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: limit
////////////////////////////////////////////////////////////////////////////////

    testLimit : function () {
      var n = query.clone().limit(10).toArray();

      assertEqual(n, numbers);

      n = query.clone().limit(9).toArray();

      assertEqual(n, numbers.slice(0,9));

      n = query.clone().limit(-9).toArray();

      assertEqual(n, numbers.slice(1,10));

      n = query.clone().limit(-1).toArray();

      assertEqual(n, numbers.slice(9,10));

      n = query.clone().limit(9).limit(-8).toArray();

      assertEqual(n, numbers.slice(1,9));

      n = query.clone().limit(-9).limit(8).toArray();
      
      assertEqual(n, numbers.slice(1,9));

      n = query.clone().limit(9).limit(8).limit(7).toArray();

      assertEqual(n, numbers.slice(0,7));

      n = query.clone().limit(-9).limit(-8).limit(-7).toArray();

      assertEqual(n, numbers.slice(3,10));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: skip and limit
////////////////////////////////////////////////////////////////////////////////

    testSkipLimit : function () {
      var n = query.clone().skip(0).limit(10).toArray();

      assertEqual(n, numbers);

      n = query.clone().limit(10).skip(0).toArray();

      assertEqual(n, numbers);

      n = query.clone().limit(9).skip(1).toArray();

      assertEqual(n, numbers.slice(1,9));

      n = query.clone().limit(9).skip(1).limit(7).toArray();

      assertEqual(n, numbers.slice(1,8));

      n = query.clone().skip(2).limit(-9).toArray();

      assertEqual(n, numbers.slice(2,10));

      n = query.clone().limit(9).skip(1).limit(-9).toArray();

      assertEqual(n, numbers.slice(1,9));

      n = query.clone().limit(-9).skip(1).limit(7).toArray();

      assertEqual(n, numbers.slice(2,9));
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                            basic skips and limits
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: skip and limit with a collection
////////////////////////////////////////////////////////////////////////////////

function SimpleQuerySkipLimitSuite () {
  var cn = "UnitTestsCollectionSkipLimit";
  var collection = null;
  var numbers = null;
  var num = function(d) { return d.n; };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  setUp : function () {
    internal.db._drop(cn);
    collection = internal.db._create(cn, { waitForSync : false });

    for (var i = 0;  i < 10;  ++i) {
      collection.save({ n : i });
    }

    numbers = collection.all().toArray().map(num);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  tearDown : function () {
    collection.drop();
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: skip
////////////////////////////////////////////////////////////////////////////////

    testSkip : function () {
      var n = collection.all().skip(0).toArray().map(num);

      assertEqual(n, numbers);

      n = collection.all().skip(1).toArray().map(num);

      assertEqual(n, numbers.slice(1,10));

      n = collection.all().skip(2).toArray().map(num);

      assertEqual(n, numbers.slice(2,10));

      n = collection.all().skip(1).skip(1).toArray().map(num);

      assertEqual(n, numbers.slice(2,10));

      n = collection.all().skip(10).toArray().map(num);
      
      assertEqual(n, []);

      n = collection.all().skip(11).toArray().map(num);

      assertEqual(n, []);

      n = collection.all().skip(9).toArray().map(num);

      assertEqual(n, [numbers[9]]);

      n = collection.all().skip(9).skip(1).toArray().map(num);

      assertEqual(n, []);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: limit
////////////////////////////////////////////////////////////////////////////////

    testLimit : function () {
      var n = collection.all().limit(10).toArray().map(num);

      assertEqual(n, numbers);

      n = collection.all().limit(9).toArray().map(num);

      assertEqual(n, numbers.slice(0,9));

      n = collection.all().limit(-9).toArray().map(num);

      assertEqual(n, numbers.slice(1,10));

      n = collection.all().limit(9).limit(-8).toArray().map(num);

      assertEqual(n, numbers.slice(1,9));

      n = collection.all().limit(-9).limit(8).toArray().map(num);
      
      assertEqual(n, numbers.slice(1,9));

      n = collection.all().limit(9).limit(8).limit(7).toArray().map(num);

      assertEqual(n, numbers.slice(0,7));

      n = collection.all().limit(-9).limit(-8).limit(-7).toArray().map(num);

      assertEqual(n, numbers.slice(3,10));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: skip and limit
////////////////////////////////////////////////////////////////////////////////

    testSkipLimit : function () {
      var n = collection.all().skip(0).limit(10).toArray().map(num);

      assertEqual(n, numbers);

      n = collection.all().limit(10).skip(0).toArray().map(num);

      assertEqual(n, numbers);

      n = collection.all().limit(9).skip(1).toArray().map(num);

      assertEqual(n, numbers.slice(1,9));

      n = collection.all().limit(9).skip(1).limit(7).toArray().map(num);

      assertEqual(n, numbers.slice(1,8));

      n = collection.all().skip(2).limit(-9).toArray().map(num);

      assertEqual(n, numbers.slice(2,10));

      n = collection.all().limit(-9).skip(1).limit(7).toArray().map(num);

      assertEqual(n, numbers.slice(2,9));
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SimpleQueryArraySkipLimitSuite);
jsunity.run(SimpleQuerySkipLimitSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
