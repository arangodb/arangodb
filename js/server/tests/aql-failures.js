/*jshint strict: true, maxlen: 400 */
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
  var count = 5000;
        
  var assertFailingQuery = function (query) {
    try {
      AQL_EXECUTE(query);
      fail();
    }
    catch (err) {
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
    
    testSingletonBlock1 : function () {
      internal.debugSetFailAt("SingletonBlock::getOrSkipSome");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testSingletonBlock2 : function () {
      internal.debugSetFailAt("SingletonBlock::getOrSkipSomeSet");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testReturnBlock : function () {
      internal.debugSetFailAt("ReturnBlock::getSome");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)");
    },

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
    
    testSortBlock3 : function () {
      internal.debugSetFailAt("SortBlock::doSortingCache");
      c.ensureSkiplist("value");
      assertFailingQuery("FOR v IN " + c.name() + " LIMIT 2 FOR w IN " + c.name() + " SORT v.value RETURN [v.value]");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testSortBlock4 : function () {
      internal.debugSetFailAt("SortBlock::doSortingNext1");
      c.ensureSkiplist("value");
      assertFailingQuery("FOR v IN " + c.name() + " LIMIT 2 FOR w IN " + c.name() + " SORT v.value RETURN [v.value]");
    },

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

    testFilterBlock1 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSome1");
      assertFailingQuery("FOR c IN " + c.name() + " FILTER c.value >= 40 FILTER c.value <= 9999 LIMIT 50, 5 RETURN c");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testFilterBlock2 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSome2");
      assertFailingQuery("LET doc = { \"_id\": \"test/76689250173\", \"_rev\": \"76689250173\", \"_key\": \"76689250173\", \"test1\": \"something\", \"test2\": { \"DATA\": [ \"other\" ] } } FOR attr IN ATTRIBUTES(doc) LET prop = doc[attr] FILTER HAS(prop, 'DATA') RETURN [ attr, prop.DATA ]"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testFilterBlock3 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSome3");
      assertFailingQuery("FOR i IN [1,2,3,4] FILTER i IN [1,2,3,4] RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testFilterBlock4 : function () {
      internal.debugSetFailAt("FilterBlock::getOrSkipSomeConcatenate");
      assertFailingQuery("FOR c IN " + c.name() + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c");
    },

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

    testCalculationBlock2 : function () {
      internal.debugSetFailAt("CalculationBlock::executeExpression");
      assertFailingQuery("FOR i IN " + c.name() + " LET v = CONCAT(i._key, 'foo') RETURN v");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testEnumerateListBlock1 : function () {
      internal.debugSetFailAt("EnumerateListBlock::getSome");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values RETURN x");
      assertFailingQuery("FOR i IN 1..10000 RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testEnumerateListBlock2 : function () {
      internal.debugSetFailAt("EnumerateListBlock::getAqlValue");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSubqueryBlock1 : function () {
      internal.debugSetFailAt("SubqueryBlock::getSome");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)");
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
    
    testExecutionBlock1 : function () {
      internal.debugSetFailAt("ExecutionBlock::inheritRegisters");
      assertFailingQuery("FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 && i.value == j.value RETURN i.value");
    },

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
      internal.debugSetFailAt("ExecutionBlock::getOrSkipSome1");
      assertFailingQuery("FOR u in " + c.name() + " SORT u.id DESC LIMIT 0,4 RETURN u");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock4 : function () {
      internal.debugSetFailAt("ExecutionBlock::getOrSkipSome2");
      assertFailingQuery("FOR u in " + c.name() + " SORT u.id DESC LIMIT " + (count - 1) + ",100 RETURN u");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock5 : function () {
      internal.debugSetFailAt("ExecutionBlock::getOrSkipSome3");
      assertFailingQuery("FOR i IN [1,2,3,4] FILTER i IN [1,2,3,4] RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testExecutionBlock6 : function () {
      internal.debugSetFailAt("ExecutionBlock::getOrSkipSomeConcatenate");
      assertFailingQuery("FOR c IN UnitTestsAhuacatlFailures FILTER c.value >= 40 FILTER c.value <= 9999 LIMIT 50, 5 RETURN c");
    },

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
    
    testIndexRangeBlock3 : function () {
      c.ensureSkiplist("value");
      internal.debugSetFailAt("IndexRangeBlock::sortConditions");
      assertFailingQuery("FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value && 10 == i.value RETURN i.value");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testIndexRangeBlock4 : function () {
      c.ensureSkiplist("value1", "value2");
      internal.debugSetFailAt("IndexRangeBlock::sortConditionsInner");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]");
    },

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

    testIndexRangeBlock6 : function () {
      c.ensureSkiplist("value");
      internal.debugSetFailAt("IndexRangeBlock::andCombineRangeInfoVecs");
      assertFailingQuery("LET a = PASSTHRU('test23') FOR i IN " + c.name() + " FILTER i._key IN APPEND([ 'test23' ], a) RETURN i._key");
    },

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

