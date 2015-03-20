/*jshint strict: true, maxlen: 200 */
/*global require, fail, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure scenarios
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
var db = arangodb.db;
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                      aql failures
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlFailureSuite () {
  "use strict";
  var cn = "UnitTestsAhuacatlFailures";
  var c;
  var count = 2000;
        
  var assertFailingQuery = function (query) {
    try {
      AQL_EXECUTE(query);
      fail();
    }
    catch (err) {
      if (err.errorNum === undefined) {
        require("internal").print("\nERROR: ", query, err, "\n");
      }
      assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
    }
  };

  return {

    setUp: function () {
      internal.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
      for (var i = 0; i < count; ++i) {
        c.save({ value: i, value2: i % 10 });
      }
    },

    tearDown: function () {
      internal.debugClearFailAt();
      db._drop(cn);
      c = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testAggregateBlock1 : function () {
      internal.debugSetFailAt("AggregatorGroup::addValues");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value INTO g RETURN [ key, g ]");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 INTO g RETURN [ key, g ]");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testAggregateBlock2 : function () {
      internal.debugSetFailAt("AggregateBlock::getOrSkipSome");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value INTO g RETURN [ key, g ]");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 INTO g RETURN [ key, g ]");
      assertFailingQuery("FOR i IN 1..10000 COLLECT key = i INTO g RETURN [ key, g ]");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testSingletonBlock1 : function () {
      internal.debugSetFailAt("SingletonBlock::getOrSkipSome");
      assertFailingQuery("LET a = 1 FOR i IN 1..10 RETURN a");
      assertFailingQuery("FOR i IN " + c.name() + " RETURN 1");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testSingletonBlock2 : function () {
      internal.debugSetFailAt("SingletonBlock::getOrSkipSomeSet");
      assertFailingQuery("FOR i IN 1..10 RETURN 1");
      assertFailingQuery("FOR i IN " + c.name() + " RETURN 1");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testReturnBlock : function () {
      internal.debugSetFailAt("ReturnBlock::getOrSkipSome");
      assertFailingQuery("RETURN 1");
      assertFailingQuery("FOR i IN 1..10 RETURN 1");
      assertFailingQuery("FOR i IN " + c.name() + " RETURN 1");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSortBlock1 : function () {
      internal.debugSetFailAt("SortBlock::doSorting");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i._key SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 SORT key RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSortBlock2 : function () {
      internal.debugSetFailAt("SortBlock::doSortingInner");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i._key SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 SORT key RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testSortBlock3 : function () {
      internal.debugSetFailAt("SortBlock::doSortingCache");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i._key SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 SORT key RETURN key");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testSortBlock4 : function () {
      internal.debugSetFailAt("SortBlock::doSortingNext1");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i._key SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 SORT key RETURN key");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSortBlock5 : function () {
      internal.debugSetFailAt("SortBlock::doSortingNext2");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i._key SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 SORT key RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testFilterBlock1 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSome1");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 7 RETURN i");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testFilterBlock2 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSome2");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 7 RETURN i");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testFilterBlock3 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSome3");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 7 RETURN i");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testFilterBlock4 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSomeConcatenate");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 7 RETURN i");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testModificationBlock : function () {
      internal.debugSetFailAt("ModificationBlock::getSome");
      assertFailingQuery("FOR i IN " + c.name() + " REMOVE i IN " + c.name());
      assertFailingQuery("FOR i IN 1..10000 REMOVE CONCAT('test' + i) IN " + c.name() + " OPTIONS { ignoreErrors: true }");
      assertFailingQuery("FOR i IN " + c.name() + " REMOVE i IN " + c.name());
      assertFailingQuery("FOR i IN 1..10000 INSERT { value3: i } IN " + c.name());

      assertEqual(count, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testCalculationBlock1 : function () {
      internal.debugSetFailAt("CalculationBlock::fillBlockWithReference");
      assertFailingQuery("FOR i IN " + c.name() + " LET v = i RETURN v");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testCalculationBlock2 : function () {
      internal.debugSetFailAt("CalculationBlock::executeExpression");
      assertFailingQuery("FOR i IN " + c.name() + " LET v = i RETURN v");
      assertFailingQuery("FOR i IN " + c.name() + " LET v = CONCAT(i._key, 'foo') RETURN v");
      assertFailingQuery("FOR i IN " + c.name() + " LET v = i._key RETURN v");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testEnumerateListBlock1 : function () {
      internal.debugSetFailAt("EnumerateListBlock::getSome");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values RETURN x");
      assertFailingQuery("FOR i IN 1..10000 RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testEnumerateListBlock2 : function () {
      internal.debugSetFailAt("EnumerateListBlock::getAqlValue");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values RETURN x");
      assertFailingQuery("FOR i IN 1..10000 RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSubqueryBlock1 : function () {
      internal.debugSetFailAt("SubqueryBlock::getSome");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values RETURN x");
      assertFailingQuery("LET values = (FOR i IN 1..10000 RETURN i) FOR x IN values RETURN x");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSubqueryBlock2 : function () {
      internal.debugSetFailAt("SubqueryBlock::executeSubquery");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values RETURN x");
      assertFailingQuery("LET values = (FOR i IN 1..10000 RETURN i) FOR x IN values RETURN x");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testExecutionBlock1 : function () {
      internal.debugSetFailAt("ExecutionBlock::inheritRegisters");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value > 10 LIMIT 99 RETURN CONCAT(i, 'foo')");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock2 : function () {
      internal.debugSetFailAt("ExecutionBlock::getBlock");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 LIMIT 9999 RETURN CONCAT(i, 'foo')");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values LIMIT 9999 RETURN x");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 9 COLLECT key = i._key LIMIT 9999 RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock3 : function () {
      internal.debugSetFailAt("ExecutionBlock::getOrSkipSome");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 LIMIT 9999 RETURN CONCAT(i, 'foo')");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values LIMIT 9999 RETURN x");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 9 COLLECT key = i._key LIMIT 9999 RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testExecutionBlock4 : function () {
      internal.debugSetFailAt("ExecutionBlock::getOrSkipSomeConcatenate");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 LIMIT 9999 RETURN CONCAT(i, 'foo')");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values LIMIT 9999 RETURN x");
      assertFailingQuery("FOR i IN " + c.name() + " LIMIT 99999 COLLECT key = i._key LIMIT 9999 RETURN key");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testEnumerateCollectionBlock : function () {
      internal.debugSetFailAt("EnumerateCollectionBlock::moreDocuments");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 9 COLLECT key = i._key RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexRangeBlock1 : function () {
      c.ensureHashIndex("value");
      internal.debugSetFailAt("IndexRangeBlock::initialize");
      assertFailingQuery("LET f = PASSTHRU(1) FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value == f RETURN i");
      assertFailingQuery("FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexRangeBlock2 : function () {
      c.ensureHashIndex("value");
      internal.debugSetFailAt("IndexRangeBlock::initializeExpressions");
      assertFailingQuery("LET f = PASSTHRU(1) FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value == f RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testIndexRangeBlock3 : function () {
      c.ensureHashIndex("value", "value2");
      internal.debugSetFailAt("IndexRangeBlock::sortConditions");
      assertFailingQuery("FOR j IN 1..10 FOR k IN 1..2 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value2 == k RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testIndexRangeBlock4 : function () {
      c.ensureHashIndex("value", "value2");
      internal.debugSetFailAt("IndexRangeBlock::sortConditionsInner");
      assertFailingQuery("FOR j IN 1..10 FOR k IN 1..2 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value2 == k RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexRangeBlock5 : function () {
      c.ensureHashIndex("value", "value2");
      internal.debugSetFailAt("IndexRangeBlock::cartesian");
      assertFailingQuery("FOR j IN 1..10 FOR k IN 1..2 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value2 == k RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
/* TODO: make this test throw errors!
    testIndexRangeBlock6 : function () {
      c.ensureHashIndex("value", "value2");
      internal.debugSetFailAt("IndexRangeBlock::andCombineRangeInfoVecs");
      assertFailingQuery("FOR j IN 1..10 FOR k IN 1..2 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value2 == k RETURN i");
    },
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexRangeBlock7 : function () {
      c.ensureSkiplist("value", "value2");
      internal.debugSetFailAt("IndexRangeBlock::readIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 25 RETURN i");
      assertFailingQuery("FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j RETURN i");
      assertFailingQuery("FOR j IN 1..10 FOR k IN 1..2 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value2 == k RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexRangeBlock8 : function () {
      c.ensureHashIndex("value", "value2");
      internal.debugSetFailAt("IndexRangeBlock::readHashIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 25 && i.value2 == 5 RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexRangeBlock9 : function () {
      c.ensureSkiplist("value", "value2");
      internal.debugSetFailAt("IndexRangeBlock::readSkiplistIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 25 && i.value2 == 5 RETURN i");
    }

  };
}
 
// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
  jsunity.run(ahuacatlFailureSuite);
}

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

