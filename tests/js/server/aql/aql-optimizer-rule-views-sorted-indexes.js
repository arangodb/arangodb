/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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

let jsunity = require("jsunity");
let db = require("@arangodb").db;

/// @brief test suite
function optimizerRuleTestSuite () {
  const ruleName = "handle-arangosearch-views";
  const cn = "UnitTestsCollection";
  const cn1 = "UnitTestsCollection1";
  const vn = "UnitTestsView";

  return {
    setUp : function () {
      db._dropView(vn);
      db._drop(cn);
      db._drop(cn1);
      
      db._create(cn, { numberOfShards: 3 });
      db._create(cn1, { numberOfShards: 3 });
    },

    tearDown : function () {
      db._dropView(vn);
      db._drop(cn);
      db._drop(cn1);
    }, 

    /// @brief test that rule has no effect
    testNoSortedness : function () {
      db._createView(vn, "arangosearch", { links: { [cn] : { includeAllFields: true } } });

      let queries = [
        "FOR doc IN " + vn + " SORT doc.value RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value ASC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc RETURN doc",
        "FOR doc IN " + vn + " SORT doc._key RETURN doc",
        "FOR doc IN " + vn + " SORT doc._key ASC RETURN doc",
        "FOR doc IN " + vn + " SORT doc._key DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value2 RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
        assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));
      });
    },

    /// @brief test that rule has no effect
    testRuleSingleAttributeNoEffect : function () {
      db._createView(vn, "arangosearch", { links: { [cn] : { includeAllFields: true } }, primarySort : [ { field : "value", direction: "asc" } ] }); 

      let queries = [ 
        "FOR doc IN " + vn + " SORT doc.value1 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 ASC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value2 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value2, doc.value1 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value2, doc.value1 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value, doc.value1 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
        assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));
      });
    },

    /// @brief test that rule has no effect
    testRuleMultipleAttributesNoEffect : function () {
      db._createView(vn, "arangosearch", { links: { [cn] : { includeAllFields: true } }, primarySort : [ { field : "value1", direction: "asc" }, { field: "value2", direction: "asc" } ] }); 

      let queries = [ 
        "FOR doc IN " + vn + " SORT doc.value1, doc.value2 DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 ASC, doc.value2 DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 DESC, doc.value2 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 DESC, doc.value2 ASC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value2, doc.value1 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value3 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value1 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value2, doc.value2 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value3, doc.value4 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value2, doc.value3 RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"), query);
        assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));
      });
    },

    /// @brief test that rule has no effect
    testRuleSingleAttributeDifferentOrderNoEffect : function () {
      db._createView(vn, "arangosearch", { links: { [cn] : { includeAllFields: true } }, primarySort : [ { field : "value", direction: "asc" } ] }); 

      let queries = [ 
        "FOR doc IN " + vn + " SORT doc.value DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value DESC, doc.value DESC RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
        assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));
      });
    },

    /// @brief test that rule has no effect
    testRuleMultipleAttributesDifferentOrderNoEffect : function () {
      db._createView(vn, "arangosearch", { links: { [cn] : { includeAllFields: true } }, primarySort : [ { field : "value1", direction: "asc" }, { field: "value2", direction: "asc" } ] }); 

      let queries = [ 
        "FOR doc IN " + vn + " SORT doc.value1 DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value2 ASC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value2 DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 ASC, doc.value2 DESC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 DESC, doc.value2 ASC RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertNotEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
        assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));
      });
    },

    /// @brief test that rule has an effect
    testRuleSingleAttributeHasEffect : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } },
        primarySort : [ { field : "value", direction: "asc" } ] 
      }); 

      let queries = [ 
        "FOR doc IN " + vn + " SORT doc.value RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value ASC RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      });
    },

    /// @brief test that rule has an effect
    testRuleMultipleAttributesHasEffect : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } }, 
        primarySort : [ { field : "value1", direction: "asc" }, { field: "value2", direction: "asc" } ] 
      }); 

      let queries = [ 
        "FOR doc IN " + vn + " SORT doc.value1 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1 ASC RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value2 RETURN doc",
        "FOR doc IN " + vn + " SORT doc.value1, doc.value2 ASC RETURN doc",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      });
    },

    /// @brief test that rule has an effect
    testRuleSingleAttributeResults : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } }, 
        primarySort : [ { field : "value", direction: "asc" } ] 
      }); 

      // insert in reverse order
      let values = [];
      for (let i = 0; i < 2000; ++i) {
        values.push({ value: "test" + (4000 - i) });
      }
      db[cn].insert(values);

      values = [];
      for (let i = 2000; i < 4000; ++i) {
        values.push({ value: "test" + (4000 - i) });
      }
      db[cn1].insert(values);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      result = AQL_EXECUTE(query).json;
      assertEqual(4000, result.length);
      let last = "";
      result.forEach(function(doc) {
        assertTrue(doc.value > last);
        last = doc.value;
      });
    },

    /// @brief test that rule has an effect
    testRuleSingleAttributeResultsDesc : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } }, 
        primarySort : [ { field : "value", direction: "desc" } ] 
      }); 
      // insert in forward order
      let values = [];
      for (let i = 0; i < 2000; ++i) {
        values.push({ value: "test" + i });
      }
      db[cn].insert(values);

      values = [];
      for (let i = 2000; i < 4000; ++i) {
        values.push({ value: "test" + i });
      }
      db[cn1].insert(values);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value DESC RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      result = AQL_EXECUTE(query).json;
      assertEqual(4000, result.length);
      let last = "test999999999";
      result.forEach(function(doc) {
        assertTrue(doc.value < last);
        last = doc.value;
      });
    },

    /// @brief test that rule has an effect
    testRuleMultipleAttributesResults : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } },
        primarySort : [ { field : "value1", direction: "asc" }, { field: "value2", direction: "asc" } ]
      }); 

      // insert in reverse order
      let values = [];
      for (let i = 0; i < 2000; ++i) {
        values.push({ value1: (4000 - i), value2: i });
      }
      db[cn].insert(values);

      values = [];
      for (let i = 2000; i < 4000; ++i) {
        values.push({ value1: (4000 - i), value2: i });
      }
      db[cn1].insert(values);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1, doc.value2 RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      result = AQL_EXECUTE(query).json;
      assertEqual(4000, result.length);
      let last1 = -1;
      let last2 = 99999;
      result.forEach(function(doc) {
        assertTrue(doc.value1 > last1);
        assertTrue(doc.value2 < last2);
        last1 = doc.value1;
        last2 = doc.value2;
      });
    },

    /// @brief test that rule has an effect
    testRuleMultipleAttributesResultsDesc : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } },
        primarySort : [ { field : "value1", direction: "desc" }, { field: "value2", direction: "asc" } ] 
      }); 

      // insert in forward order
      let values = [];
      for (let i = 0; i < 2000; ++i) {
        values.push({ value1: i, value2: i });
      }
      db[cn].insert(values);

      values = [];
      for (let i = 2000; i < 4000; ++i) {
        values.push({ value1: i, value2: i });
      }
      db[cn1].insert(values);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      result = AQL_EXECUTE(query).json;
      assertEqual(4000, result.length);
      let last1 = 99999;
      let last2 = 99999;
      result.forEach(function(doc) {
        assertTrue(doc.value1 < last1);
        assertTrue(doc.value2 < last2);
        last1 = doc.value1;
        last2 = doc.value2;
      });
    },

    /// @brief test that rule has an effect
    testRuleSameAttributeValuesDesc : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } },
        primarySort : [ { field : "value1", direction: "desc" }, { field: "value2", direction: "asc" } ] 
      }); 

      // insert in forward order
      let values = [];
      for (let i = 0; i < 2000; ++i) {
        values.push({ value1: "same", value2: (4000 - i) });
      }
      db[cn].insert(values);

      values = [];
      for (let i = 2000; i < 4000; ++i) {
        values.push({ value1: "same", value2: (4000 - i) });
      }
      db[cn1].insert(values);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      result = AQL_EXECUTE(query).json;
      assertEqual(4000, result.length);
      let last2 = -1;
      result.forEach(function(doc) {
        assertEqual("same", doc.value1);
        assertTrue(doc.value2 > last2);
        last2 = doc.value2;
      });
    },

    /// @brief test that rule has an effect
    testRuleSameAttributeValuesDescFullCount : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } },
        primarySort : [ { field : "value1", direction: "desc" }, { field: "value2", direction: "asc" } ] 
      }); 

      // insert in forward order
      let values = [];
      for (let i = 0; i < 2000; ++i) {
        values.push({ value1: "same", value2: (4000 - i) });
      }
      db[cn].insert(values);

      values = [];
      for (let i = 2000; i < 4000; ++i) {
        values.push({ value1: "same", value2: (4000 - i) });
      }
      db[cn1].insert(values);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      result = AQL_EXECUTE(query, {}, { fullCount: true });
      assertEqual(4000, result.json.length);
      assertEqual(4000, result.stats.fullCount);
      let last2 = -1;
      result.json.forEach(function(doc) {
        assertEqual("same", doc.value1);
        assertTrue(doc.value2 > last2);
        last2 = doc.value2;
      });
    },

    /// @brief test that rule has an effect
    testRuleMultipleAttributesResultsDescWithOffset : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } },
        primarySort : [ { field : "value1", direction: "desc" }, { field: "value2", direction: "asc" } ] 
      }); 

      // insert in forward order
      let firstHalf = [];
      for (let i = 0; i < 2000; ++i) {
        firstHalf.push({ value1: i, value2: i });
      }
      db[cn].insert(firstHalf);

      let secondHalf = [];
      for (let i = 2000; i < 4000; ++i) {
        secondHalf.push({ value1: i, value2: i });
      }
      db[cn1].insert(secondHalf);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 LIMIT 2000, 100000 RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      firstHalf.sort(function(lhs, rhs) {
        if (lhs.value1 === rhs.value1) {
          if (lhs.value2 === rhs.value2) {
            return 0;
          }

          if (lhs.value2 < rhs.value2) {
            return -1;
          }

          return 1;
        }

        if (lhs.value1 < rhs.value1) {
          return 1;
        }

        return -1;
      });

      result = AQL_EXECUTE(query).json;
      assertEqual(firstHalf.length, result.length);
      let last1 = 99999;
      let last2 = 99999;
      let i = 0;
      result.forEach(function(doc) {
        assertEqual(doc.value1, firstHalf[i].value1);
        assertEqual(doc.value2, firstHalf[i].value2);
        assertTrue(doc.value1 < last1);
        assertTrue(doc.value2 < last2);
        last1 = doc.value1;
        last2 = doc.value2;
        ++i;
      });
    },

    testRuleMultipleAttributesResultsDescWithOffsetFullCount : function () {
      db._createView(vn, "arangosearch", { 
        links: { [cn] : { includeAllFields: true }, [cn1] : { includeAllFields: true } },
        primarySort : [ { field : "value1", direction: "desc" }, { field: "value2", direction: "asc" } ] 
      }); 

      // insert in forward order
      let firstHalf = [];
      for (let i = 0; i < 2000; ++i) {
        firstHalf.push({ value1: i, value2: i });
      }
      db[cn].insert(firstHalf);

      let secondHalf = [];
      for (let i = 2000; i < 4000; ++i) {
        secondHalf.push({ value1: i, value2: i });
      }
      db[cn1].insert(secondHalf);

      let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 LIMIT 2000, 100 RETURN doc";
      let result = AQL_EXPLAIN(query);
      assertEqual(-1, result.plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      firstHalf.sort(function(lhs, rhs) {
        if (lhs.value1 === rhs.value1) {
          if (lhs.value2 === rhs.value2) {
            return 0;
          }

          if (lhs.value2 < rhs.value2) {
            return -1;
          }

          return 1;
        }

        if (lhs.value1 < rhs.value1) {
          return 1;
        }

        return -1;
      });

      result = AQL_EXECUTE(query, {}, { fullCount: true});
      assertEqual(100, result.json.length);
      assertEqual(4000, result.stats.fullCount);
      let last1 = 99999;
      let last2 = 99999;
      let i = 0;
      result.json.forEach(function(doc) {
        assertEqual(doc.value1, firstHalf[i].value1);
        assertEqual(doc.value2, firstHalf[i].value2);
        assertTrue(doc.value1 < last1);
        assertTrue(doc.value2 < last2);
        last1 = doc.value1;
        last2 = doc.value2;
        ++i;
      });
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
