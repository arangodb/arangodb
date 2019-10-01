/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
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

var db = require("@arangodb").db;
var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlClusterJoinKeySuite () {
  var cn1 = "UnitTestsAhuacatlSubquery1";
  var cn2 = "UnitTestsAhuacatlSubquery2";
  var c1, c2;

  var sorter = function (l, r) {
    if (l !== r) {
      return l - r;
    }
    return 0;
  };
  
  var sorter2 = function (l, r) {
    if (l[0] !== r[0]) {
      return l[0] - r[0];
    }
    return 0;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1, { numberOfShards: 5 });
      c2 = db._create(cn2, { numberOfShards: 4 });

      for (var i = 0; i < 20; ++i) {
        c1.save({ _key: "test" + i, value: i });
        c2.save({ _key: "test" + i, value: i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsDifferentCollectionsNonKey : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsDifferentCollectionsNonKeyFilter : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter);
      assertEqual([ 1, 2, 4 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsDifferentCollectionsKey : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FOR c2 IN @@cn2 FILTER c1._key == c2._key RETURN c2.value", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsDifferentCollectionsKeyFilter : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] FOR c2 IN @@cn2 FILTER c1._key == c2._key RETURN c2.value", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter);
      assertEqual([ 1, 2, 4 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsSameCollection : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] FOR c2 IN @@cn2 FILTER c1._key == c2._key RETURN c2.value", { "@cn1": cn1, "@cn2": cn1 }); // same collection
      actual.json.sort(sorter);
      assertEqual([ 1, 2, 4 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsSameCollectionNonShardKey : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value", { "@cn1": cn1, "@cn2": cn1 }); // same collection
      actual.json.sort(sorter);
      assertEqual([ 1, 2, 4 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDifferentCollectionsKey : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] LET match = (FOR c2 IN @@cn2 FILTER c1._key == c2._key RETURN c2.value) RETURN match", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter2);
      assertEqual([ [ 1 ], [ 2 ], [ 4 ] ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDifferentCollectionsNonKey : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] LET match = (FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value) RETURN match", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter2);
      assertEqual([ [ 1 ], [ 2 ], [ 4 ] ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testSubquerySameCollectionKey : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] LET match = (FOR c2 IN @@cn2 FILTER c1._key == c2._key RETURN c2.value) RETURN match", { "@cn1": cn1, "@cn2": cn1 }); // same collection
      actual.json.sort(sorter2);
      assertEqual([ [ 1 ], [ 2 ], [ 4 ] ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testSubquerySameCollectionNonKey : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] LET match = (FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value) RETURN match", { "@cn1": cn1, "@cn2": cn1 }); // same collection
      actual.json.sort(sorter2);
      assertEqual([ [ 1 ], [ 2 ], [ 4 ] ], actual.json);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlClusterJoinNonKeySuite () {
  var cn1 = "UnitTestsAhuacatlSubquery1";
  var cn2 = "UnitTestsAhuacatlSubquery2";
  var c1, c2;
  
  var sorter = function (l, r) {
    if (l !== r) {
      return l - r;
    }
    return 0;
  };
  
  var sorter2 = function (l, r) {
    if (l[0] !== r[0]) {
      return l[0] - r[0];
    }
    return 0;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1, { numberOfShards: 5, shardKeys: [ "value" ] });
      c2 = db._create(cn2, { numberOfShards: 4, shardKeys: [ "value" ] });

      for (var i = 0; i < 20; ++i) {
        c1.save({ value: i });
        c2.save({ value: i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsDifferentCollectionsNonKeyJNKS : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsDifferentCollectionsNonKeyFilterJNKS : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter);
      assertEqual([ 1, 2, 4 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testNestedLoopsSameCollectionNonShardKeyJNKS : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value", { "@cn1": cn1, "@cn2": cn1 }); // same collection
      actual.json.sort(sorter);
      assertEqual([ 1, 2, 4 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDifferentCollectionsNonKeyJNKS : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] LET match = (FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value) RETURN match", { "@cn1": cn1, "@cn2": cn2 });
      actual.json.sort(sorter2);
      assertEqual([ [ 1 ], [ 2 ], [ 4 ] ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testSubquerySameCollectionNonKeyJNKS : function () {
      var actual = AQL_EXECUTE("FOR c1 IN @@cn1 FILTER c1.value IN [ 1, 2, 4 ] LET match = (FOR c2 IN @@cn2 FILTER c1.value == c2.value RETURN c2.value) RETURN match", { "@cn1": cn1, "@cn2": cn1 }); // same collection
      actual.json.sort(sorter2);
      assertEqual([ [ 1 ], [ 2 ], [ 4 ] ], actual.json);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlClusterJoinKeySuite);
jsunity.run(ahuacatlClusterJoinNonKeySuite);

return jsunity.done();

