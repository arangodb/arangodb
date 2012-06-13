/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require,
    db,
    assertEqual, assertTrue,
    ArangoCollection, ArangoEdgesCollection */

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
var internal = require("internal");

var SQB = require("simple-query-basics");

require("simple-query");

// -----------------------------------------------------------------------------
// --SECTION--                                  basic skips and limits for array
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

      n = query.clone().limit(9).limit(8).limit(7).toArray();

      assertEqual(n, numbers.slice(0,7));
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

      n = query.clone().skip(-5).limit(3).toArray();

      assertEqual(n, numbers.slice(5,8));

      n = query.clone().skip(-8).limit(7).skip(1).limit(4).toArray();

      assertEqual(n, numbers.slice(3,7));

      n = query.clone().skip(-10).limit(9).skip(1).limit(7).toArray();

      assertEqual(n, numbers.slice(1,8));
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                    basic skips and limits for all
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: skip and limit with a collection
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryAllSkipLimitSuite () {
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

    testAllSkip : function () {
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

    testAllLimit : function () {
      var n = collection.all().limit(10).toArray().map(num);

      assertEqual(n, numbers);

      n = collection.all().limit(9).toArray().map(num);

      assertEqual(n, numbers.slice(0,9));

      n = collection.all().limit(9).limit(8).limit(7).toArray().map(num);

      assertEqual(n, numbers.slice(0,7));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: skip and limit
////////////////////////////////////////////////////////////////////////////////

    testAllSkipLimit : function () {
      var n = collection.all().skip(0).limit(10).toArray().map(num);

      assertEqual(n, numbers);

      n = collection.all().limit(10).skip(0).toArray().map(num);

      assertEqual(n, numbers);

      n = collection.all().limit(9).skip(1).toArray().map(num);

      assertEqual(n, numbers.slice(1,9));

      n = collection.all().limit(9).skip(1).limit(7).toArray().map(num);

      assertEqual(n, numbers.slice(1,8));

      n = collection.all().skip(-5).limit(3).toArray().map(num);

      assertEqual(n, numbers.slice(5,8));

      n = collection.all().skip(-8).limit(7).skip(1).limit(4).toArray().map(num);

      assertEqual(n, numbers.slice(3,7));

      n = collection.all().skip(-10).limit(9).skip(1).limit(7).toArray().map(num);

      assertEqual(n, numbers.slice(1,8));
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                         basic tests for byExample
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: query-by-example
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExampleSuite () {
  var cn = "UnitTestsCollectionByExample";
  var collection = null;
  var id = function(d) { return d._id; };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: byExample
////////////////////////////////////////////////////////////////////////////////

    testByExampleObject : function () {
      var d1 = collection.save({ i : 1 });
      var d2 = collection.save({ i : 1, a : { j : 1 } });
      var d3 = collection.save({ i : 1, a : { j : 1, k : 1 } });
      var d4 = collection.save({ i : 1, a : { j : 2, k : 2 } });
      var d5 = collection.save({ i : 2 });
      var d6 = collection.save({ i : 2, a : 2 });
      var d7 = collection.save({ i : 2, a : { j : 2, k : 2 } });
      var s;

      s = collection.byExample({ i : 1 }).toArray().map(id).sort();
      assertEqual([d1._id, d2._id, d3._id, d4._id].sort(), s);

      s = collection.byExample({ i : 2 }).toArray().map(id).sort();
      assertEqual([d5._id, d6._id, d7._id].sort(), s);

      s = collection.byExample({ a : { j : 1 } }).toArray().map(id).sort();
      assertEqual([d2._id], s);

      s = collection.byExample({ "a.j" : 1 }).toArray().map(id).sort();
      assertEqual([d2._id, d3._id].sort(), s);

      s = collection.byExample({ i : 2, "a.k" : 2 }).toArray().map(id).sort();
      assertEqual([d7._id], s);

      s = collection.firstExample({ "i" : 2, "a.k" : 2 });
      assertEqual(d7._id, s._id);

      s = collection.firstExample({ "i" : 2, "a.k" : 27 });
      assertEqual(null, s);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: byExample
////////////////////////////////////////////////////////////////////////////////

    testByExampleList : function () {
      var d1 = collection.save({ i : 1 });
      var d2 = collection.save({ i : 1, a : { j : 1 } });
      var d3 = collection.save({ i : 1, a : { j : 1, k : 1 } });
      var d4 = collection.save({ i : 1, a : { j : 2, k : 2 } });
      var d5 = collection.save({ i : 2 });
      var d6 = collection.save({ i : 2, a : 2 });
      var d7 = collection.save({ i : 2, a : { j : 2, k : 2 } });
      var s;

      s = collection.byExample("i", 1).toArray().map(id).sort();
      assertEqual([d1._id, d2._id, d3._id, d4._id].sort(), s);

      s = collection.byExample("i", 2).toArray().map(id).sort();
      assertEqual([d5._id, d6._id, d7._id].sort(), s);

      s = collection.byExample("a", { j : 1 }).toArray().map(id).sort();
      assertEqual([d2._id], s);

      s = collection.byExample("a.j", 1).toArray().map(id).sort();
      assertEqual([d2._id, d3._id].sort(), s);

      s = collection.byExample("i", 2, "a.k", 2).toArray().map(id).sort();
      assertEqual([d7._id], s);

      s = collection.firstExample("i", 2, "a.k", 2);
      assertEqual(d7._id, s._id);

      s = collection.firstExample("i", 2, "a.k", 27);
      assertEqual(null, s);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                             basic tests for range
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: range
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryRangeSuite () {
  var cn = "UnitTestsCollectionRange";
  var collection = null;
  var age = function(d) { return d.age; };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });

      for (var i = 0;  i < 100;  ++i) {
        collection.save({ age : i });
      }

      collection.ensureSkiplist("age");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: range
////////////////////////////////////////////////////////////////////////////////

    testRange : function () {
      var l = collection.range("age", 10, 13).toArray().map(age).sort();
      assertEqual([10,11,12], l);

      l = collection.closedRange("age", 10, 13).toArray().map(age).sort();
      assertEqual([10,11,12,13], l);
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                      basic tests for unique range
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: range
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryUniqueRangeSuite () {
  var cn = "UnitTestsCollectionRange";
  var collection = null;
  var age = function(d) { return d.age; };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });

      for (var i = 0;  i < 100;  ++i) {
        collection.save({ age : i });
      }

      collection.ensureUniqueSkiplist("age");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: range
////////////////////////////////////////////////////////////////////////////////

    testRange : function () {
      var l = collection.range("age", 10, 13).toArray().map(age).sort();
      assertEqual([10,11,12], l);

      l = collection.closedRange("age", 10, 13).toArray().map(age).sort();
      assertEqual([10,11,12,13], l);
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
jsunity.run(SimpleQueryAllSkipLimitSuite);
jsunity.run(SimpleQueryByExampleSuite);
jsunity.run(SimpleQueryRangeSuite);
jsunity.run(SimpleQueryUniqueRangeSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
