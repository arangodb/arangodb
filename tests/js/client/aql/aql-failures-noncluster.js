/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Michael Hackstein
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");
let IM = global.instanceManager;

function ahuacatlFailureSuite () {
  'use strict';
  const cn = "UnitTestsAhuacatlFailures";
  const en = "UnitTestsAhuacatlEdgeFailures";
  const count = 5000;
  let idx = null;
  let c;
  let e;
        
  const assertFailingQuery = function (query, rulesToExclude) {
    if (!rulesToExclude) {
      rulesToExclude = [];
    }
    try {
      db._query(query, null, { optimizer: { rules: rulesToExclude } });
      fail();
    } catch (err) {
      assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, query);
    }
  };

  return {
    setUpAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
      db._drop(en);
      e = db._createEdgeCollection(en);
      let docs = [];
      for (var i = 0; i < count; ++i) {
        docs.push({ _key: String(i), value: i, value2: i % 10 });
      }
      c.insert(docs);
      docs = [];
      for (var j = 0; j < count / 10; ++j) {
        docs.push({'_from': cn + "/" + j, '_to': cn + "/" + (j + 1) });
      }
      e.insert(docs);
    },

    setUp: function () {
      idx = null;
    },

    tearDownAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = null;
      db._drop(en);
      e = null;
    },

    tearDown: function() {
      IM.debugClearFailAt();
      if (idx != null) {
        db._dropIndex(idx);
        idx = null;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test error during plan serialization
////////////////////////////////////////////////////////////////////////////////
    
    testProfileSerialization : function () {
      ["Query::serializePlans1", "Query::serializePlans2"].forEach((where) => {
        IM.debugClearFailAt();
        IM.debugSetFailAt(where);

        try {
          db._profileQuery(`FOR doc IN ${cn} FILTER doc._key == 'test' RETURN doc`);
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }
      });
    },

    testPlanSerialization : function () {
      ["Query::serializePlans1", "Query::serializePlans2"].forEach((where) => {
        IM.debugClearFailAt();
        IM.debugSetFailAt(where);

        try {
          db._explain(`FOR doc IN ${cn} FILTER doc._key == 'test' RETURN doc`);
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }
      });
    },
    
    testAllPlansSerialization : function () {
      ["Query::serializePlans1", "Query::serializePlans2"].forEach((where) => {
        IM.debugClearFailAt();
        IM.debugSetFailAt(where);

        try {
          db._explain(`FOR doc IN ${cn} FILTER doc._key == 'test' RETURN doc`, null, {allPlans: true});
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test UNION for memleaks
////////////////////////////////////////////////////////////////////////////////

    testAqlFunctionUnion : function () {
      var where = [ "AqlFunctions::OutOfMemory1", "AqlFunctions::OutOfMemory2", "AqlFunctions::OutOfMemory3" ];

      where.forEach(function(w) {
        IM.debugClearFailAt();
       
        IM.debugSetFailAt(w);
        assertFailingQuery("RETURN NOOPT(UNION([ 1, 2, 3, 4 ], [ 5, 6, 7, 8 ], [ 9, 10, 11 ]))");
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test UNION_DISTINCT for memleaks
////////////////////////////////////////////////////////////////////////////////

    testAqlFunctionUnionDistinct : function () {
      var where = [ "AqlFunctions::OutOfMemory1", "AqlFunctions::OutOfMemory2", "AqlFunctions::OutOfMemory3" ];

      where.forEach(function(w) {
        IM.debugClearFailAt();
       
        IM.debugSetFailAt(w);
        assertFailingQuery("RETURN NOOPT(UNION_DISTINCT([ 1, 2, 3, 4 ], [ 5, 6, 7, 8 ], [ 9, 10, 11 ]))");
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test INTERSECTION for memleaks
////////////////////////////////////////////////////////////////////////////////

    testAqlFunctionIntersection : function () {
      var where = [ "AqlFunctions::OutOfMemory1", "AqlFunctions::OutOfMemory2", "AqlFunctions::OutOfMemory3" ];

      where.forEach(function(w) {
        IM.debugClearFailAt();
       
        IM.debugSetFailAt(w);
        assertFailingQuery("RETURN NOOPT(INTERSECTION([ 1, 2, 3, 4 ], [ 5, 6, 7, 8 ], [ 9, 10, 11 ]))");
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSortedAggregateBlock1 : function () {
      IM.debugSetFailAt("CollectGroup::addValues");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSortedAggregateBlock2 : function () {
      IM.debugSetFailAt("SortedCollectBlock::getOrSkipSome");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
      assertFailingQuery("FOR i IN 1..10000 COLLECT key = i INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");

      IM.debugClearFailAt();
      IM.debugSetFailAt("SortedCollectBlock::getOrSkipSomeOuter");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
      assertFailingQuery("FOR i IN 1..10000 COLLECT key = i INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSortedAggregateBlock3 : function () {
      IM.debugSetFailAt("SortedCollectBlock::hasMore");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
      assertFailingQuery("FOR i IN 1..10000 COLLECT key = i INTO g OPTIONS { method: 'sorted' } RETURN [ key, g ]");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testHashedAggregateBlock : function () {
      IM.debugSetFailAt("HashedCollectExecutor::produceRows");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 RETURN key");
      assertFailingQuery("FOR i IN 1..10000 COLLECT key = i RETURN key");
      
      IM.debugClearFailAt();
      IM.debugSetFailAt("HashedCollectExecutor::produceRows");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 RETURN key");
      assertFailingQuery("FOR i IN 1..10000 COLLECT key = i RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testSingletonBlock1 : function () {
      IM.debugSetFailAt("SingletonBlock::getOrSkipSome");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q * year)) RETURN UNIQUE(quarters)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testSingletonBlock2 : function () {
      IM.debugSetFailAt("SingletonBlock::getOrSkipSomeSet");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q * year)) RETURN UNIQUE(quarters)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testSortBlock1 : function () {
      IM.debugSetFailAt("SortBlock::doSorting");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i._key SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value SORT key RETURN key");
      assertFailingQuery("FOR i IN " + c.name() + " COLLECT key = i.value2 SORT key RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testFilterBlock2 : function () {
      IM.debugSetFailAt("FilterExecutor::produceRows");
      assertFailingQuery("LET doc = { \"_id\": \"test/76689250173\", \"_rev\": \"76689250173\", \"_key\": \"76689250173\", \"test1\": \"something\", \"test2\": { \"DATA\": [ \"other\" ] } } FOR attr IN ATTRIBUTES(doc) LET prop = doc[attr] FILTER HAS(prop, 'DATA') RETURN [ attr, prop.DATA ]"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testFilterBlock3 : function () {
      IM.debugSetFailAt("FilterExecutor::produceRows");
      assertFailingQuery("FOR i IN [1,2,3,4] FILTER i IN NOOPT([1,2,3,4]) RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testCalculationBlock1 : function () {
      IM.debugSetFailAt("CalculationBlock::fillBlockWithReference");
      assertFailingQuery("FOR i IN " + c.name() + " LET v = i RETURN v", [ "-all" ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testCalculationBlock2 : function () {
      IM.debugSetFailAt("CalculationBlock::executeExpression");
      assertFailingQuery("FOR i IN " + c.name() + " LET v = NOOPT(CONCAT(i._key, 'foo')) RETURN v");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testEnumerateListBlock1 : function () {
      IM.debugSetFailAt("EnumerateListBlock::getSome");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values RETURN x", [ "-all" ]);
      assertFailingQuery("FOR i IN 1..10000 RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testEnumerateListBlock2 : function () {
      IM.debugSetFailAt("EnumerateListBlock::getAqlValue");
      assertFailingQuery("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] RETURN q)) RETURN LENGTH(quarters)", [ "-all" ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////
    
    testExecutionBlock1 : function () {
      IM.debugSetFailAt("ExecutionBlock::inheritRegisters");
      assertFailingQuery("FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 && i.value == j.value RETURN i.value");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock2 : function () {
      IM.debugSetFailAt("ExecutionBlock::getBlock");
      assertFailingQuery("FOR i IN 1..10000 FILTER i.value % 4 == 0 LIMIT 9999 RETURN CONCAT(i, 'foo')");
      assertFailingQuery("LET values = (FOR i IN " + c.name() + " RETURN i) FOR x IN values LIMIT 9999 RETURN x");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 9 COLLECT key = i._key LIMIT 9999 RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock3 : function () {
      IM.debugSetFailAt("ExecutionBlock::getOrSkipSome1");
      assertFailingQuery("FOR u in " + c.name() + " SORT u.id DESC LIMIT 0,4 RETURN u", ['-sort-limit']);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock4 : function () {
      IM.debugSetFailAt("ExecutionBlock::getOrSkipSome2");
      assertFailingQuery("FOR u in " + c.name() + " SORT u.id DESC LIMIT " + (count - 1) + ",100 RETURN u", ['-sort-limit']);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testExecutionBlock5 : function () {
      IM.debugSetFailAt("ExecutionBlock::getOrSkipSome3");
      assertFailingQuery("FOR i IN [1,2,3,4] FILTER i IN [1,2,3,4] RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testEnumerateCollectionBlock : function () {
      IM.debugSetFailAt("EnumerateCollectionBlock::moreDocuments");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value2 == 9 COLLECT key = i.value2 RETURN key");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexBlock1 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("IndexBlock::initialize");
      assertFailingQuery("LET f = NOOPT(1) FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value == f RETURN i");
      assertFailingQuery("FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexBlock2 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("IndexBlock::initializeExpressions");
      assertFailingQuery("LET f = NOOPT(1) FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value == f RETURN i");
    },

    testIndexBlock3 : function () {
      idx = c.ensureIndex({ type: "skiplist", fields: ["value", "value2"] });
      IM.debugSetFailAt("IndexBlock::readIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 25 RETURN i");
      assertFailingQuery("FOR j IN 1..10 FOR i IN " + c.name() + " FILTER i.value == j RETURN i");
      assertFailingQuery("FOR j IN 1..10 FOR k IN 1..2 FOR i IN " + c.name() + " FILTER i.value == j FILTER i.value2 == k RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexBlock4 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value", "value2"] });
      IM.debugSetFailAt("IndexBlock::readIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 25 && i.value2 == 5 RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testIndexBlock5 : function () {
      idx = c.ensureIndex({ type: "skiplist", fields: ["value", "value2"] });
      IM.debugSetFailAt("IndexBlock::readIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 25 && i.value2 == 5 RETURN i");
    },

    testIndexBlock6 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("IndexBlock::executeExpression");
      // CONCAT  is an arbitrary non v8 function and can be replaced
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == NOOPT(CONCAT('1','2')) RETURN i");
    },

    testConditionFinder1 : function () {
      IM.debugSetFailAt("ConditionFinder::insertIndexNode");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i._key == 'test' RETURN i");
    },

    testConditionFinder2 : function () {
      IM.debugSetFailAt("ConditionFinder::normalizePlan");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i._key == 'test' RETURN i");
    },

    testConditionFinder3 : function () {
      IM.debugSetFailAt("ConditionFinder::sortNode");
      assertFailingQuery("FOR i IN " + c.name() + " SORT i._key RETURN i");
    },

    testConditionFinder4 : function () {
      IM.debugSetFailAt("ConditionFinder::variableDefinition");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i._key == 'test' RETURN i");
    },

    testIndexNodeSkiplist2 : function () {
      idx = c.ensureIndex({ type: "skiplist", fields: ["value"] });
      IM.debugSetFailAt("SkiplistIndex::noIterator");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 1 RETURN i");
    },

    testIndexNodeSkiplist3 : function () {
      idx = c.ensureIndex({ type: "skiplist", fields: ["value"] });
      IM.debugSetFailAt("SkiplistIndex::permutationEQ");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 1 RETURN i");
    },

    testIndexNodeSkiplist4 : function () {
      idx = c.ensureIndex({ type: "skiplist", fields: ["value"] });
      IM.debugSetFailAt("Index::permutationIN");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value IN [1, 2] RETURN i");
    },

    testIndexNodeSkiplist5 : function () {
      idx = c.ensureIndex({ type: "skiplist", fields: ["value[*]"] });
      IM.debugSetFailAt("SkiplistIndex::permutationArrayIN");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER 1 IN i.value[*] RETURN i");
    },

    testIndexNodeSkiplist6 : function () {
      idx = c.ensureIndex({ type: "skiplist", fields: ["value"] });
      IM.debugSetFailAt("SkiplistIndex::accessFitsIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 1 RETURN i");
    },

    testIndexNodeHashIndex1 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("HashIndex::noIterator");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 1 RETURN i");
    },

    testIndexNodeHashIndex2 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("HashIndex::permutationEQ");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 1 RETURN i");
    },

    testIndexNodeHashIndex3 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("Index::permutationIN");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value IN [1, 2] RETURN i");
    },

    testIndexNodeHashIndex4 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value[*]"] });
      IM.debugSetFailAt("HashIndex::permutationArrayIN");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER 1 IN i.value[*] RETURN i");
    },

    testSimpleAttributeMatcher2 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("SimpleAttributeMatcher::specializeAllChildrenEQ");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 1 RETURN i");
    },

    testSimpleAttributeMatcher3 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("SimpleAttributeMatcher::specializeAllChildrenIN");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value IN [1, 2] RETURN i");
    },

    testSimpleAttributeMatcher4 : function () {
      idx = c.ensureIndex({ type: "hash", fields: ["value"] });
      IM.debugSetFailAt("SimpleAttributeMatcher::accessFitsIndex");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i.value == 1 RETURN i");
    },

    testIndexNodePrimaryIndex1 : function () {
      IM.debugSetFailAt("PrimaryIndex::iteratorValNodes");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i._key IN ['1', '2'] RETURN i");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i._id IN ['" + c.name() + "/1', '" + c.name() + "/2'] RETURN i");
    },

    testIndexNodePrimaryIndex2 : function () {
      IM.debugSetFailAt("PrimaryIndex::noIterator");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i._key == '1' RETURN i");
      assertFailingQuery("FOR i IN " + c.name() + " FILTER i._id == '" + c.name() + "/1' RETURN i");
    },

    testIndexNodeEdgeIndex1 : function () {
      IM.debugSetFailAt("EdgeIndex::noIterator");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._from == '" + c.name() + "/1'  RETURN i");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._to == '" + c.name() + "/1'  RETURN i");
    },

    testIndexNodeEdgeIndex2 : function () {
      IM.debugSetFailAt("EdgeIndex::collectKeys");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._from == '" + c.name() + "/1'  RETURN i");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._to == '" + c.name() + "/1'  RETURN i");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._from IN ['" + c.name() + "/1', '" + c.name() + "/2']  RETURN i");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._to IN ['" + c.name() + "/1', '" + c.name() + "/2']  RETURN i");
    },

    testIndexNodeEdgeIndex3 : function () {
      IM.debugSetFailAt("EdgeIndex::iteratorValNodes");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._from IN ['" + c.name() + "/1', '" + c.name() + "/2']  RETURN i");
      assertFailingQuery("FOR i IN " + e.name() + " FILTER i._to IN ['" + c.name() + "/1', '" + c.name() + "/2']  RETURN i");
    }

  };
}
 
function ahuacatlFailureModifySuite () {
  'use strict';
  var cn = "UnitTestsAhuacatlFailures";
  var en = "UnitTestsAhuacatlEdgeFailures";
  var c;
  var e;
  var count = 5000;
        
  var assertFailingQuery = function (query, rulesToExclude) {
    if (!rulesToExclude) {
      rulesToExclude = [];
    }
    try {
      db._query(query, null, { optimizer: { rules: rulesToExclude } });
      fail();
    } catch (err) {
      assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, query);
    }
  };

  return {

    setUpAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);
      db._drop(en);
      e = db._createEdgeCollection(en);
      let docs = [];
      for (var i = 0; i < count; ++i) {
        docs.push({ _key: String(i), value: i, value2: i % 10 });
      }
      c.insert(docs);
      docs = [];
      for (var j = 0; j < count / 10; ++j) {
        docs.push({'_from': cn + "/" + j, '_to': cn + "/" + (j + 1) });
      }
      e.insert(docs);
    },

    tearDownAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      c = null;
      db._drop(en);
      e = null;
    },

    tearDown: function() {
      IM.debugClearFailAt();
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test failure
////////////////////////////////////////////////////////////////////////////////

    testModificationBlock : function () {
      IM.debugSetFailAt("ModificationBlock::getSome");
      assertFailingQuery("FOR i IN " + c.name() + " REMOVE i IN " + c.name());
      assertFailingQuery("FOR i IN 1..10000 REMOVE CONCAT('test' + i) IN " + c.name() + " OPTIONS { ignoreErrors: true }");
      assertFailingQuery("FOR i IN " + c.name() + " REMOVE i IN " + c.name());
      assertFailingQuery("FOR i IN 1..10000 INSERT { value3: i } IN " + c.name());

      assertEqual(count, c.count());
    }
  };
}

jsunity.run(ahuacatlFailureModifySuite);
jsunity.run(ahuacatlFailureSuite);
return jsunity.done();
