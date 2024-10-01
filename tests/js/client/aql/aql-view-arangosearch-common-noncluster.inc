/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNotUndefined, fail*/

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
(function() {

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const analyzers = require("@arangodb/analyzers");
const ERRORS = require("@arangodb").errors;
const longValue = "VeryLongValueToNotFitIntoSSOAndTriggerBufferCopyingIssuesIfAnyBlaBlaBla";

return function (args) {
  function getNodes(query, type, bindVars, options) {
    let stmt = db._createStatement(query);
    if (typeof bindVars === "object") {
      stmt.bind(bindVars);
    }
    if (typeof options === "object") {
      stmt.setOptions(options);
    }
    return stmt.explain()
             .plan
             .nodes
             .filter(node => node.type === type);
  }
  
  return {
    internal: {
       createCollectionsAndData: function() {
          db._drop("AuxUnitTestsCollection");
          let auxCol = db._create("AuxUnitTestsCollection");
          auxCol.save({ foobar: ['foo', 'bar'], foo: ['foo'], bar:['bar'], empty: []});

          db._drop("AnotherUnitTestsCollection");
          let ac = db._create("AnotherUnitTestsCollection");
          ac.save({ a: "foo", id : 0 });
          ac.save({ a: "ba", id : 1 });

          db._drop("UnitTestsWithArrayCollection");
          let arrayCol = db._create("UnitTestsWithArrayCollection");
          arrayCol.save({ c: 0, a: ['foo', 'bar', 'baz']});
          // this will allow to catch if accidentally "all" filter will be used
          arrayCol.save({ c: 1, a: ['afoo', 'abar', 'abaz']});
          db._drop("TestsCollectionWithManyFields");
          let mfc = db._create("TestsCollectionWithManyFields");
          mfc.save({field1:"1value", field2:"2value", field3: 1, field4: 11111, field5: 1, field6: 1});
          mfc.save({field1:"1value1", field2:"2value1", field3: 2, field4: 11112, field5: 2, field6: 2});
          mfc.save({field1:"1value2", field2:"2value2", field3: 3, field4: 11113, field5: 3, field6: 3});
          mfc.save({field1:"1value3", field2:"2value3", field3: 4, field4: 11114, field5: 4, field6: 4});
          
          db._drop("TestsCollectionWithLongFields");
          let longData = [];
          let lfc = db._create("TestsCollectionWithLongFields");
          for (let k = 0; k < 1500; ++k) {
            longData.push({field1:longValue + k, field2: longValue, field3: k});
          }
          lfc.save(longData);
          {
              let queryColl = "DisjunctionCollection";
              db._drop(queryColl);
           
              let coll = db._create(queryColl);
              let documents = [
                {"value":"test"},
                // these docs will make STARTS_WITH more "costly" than PHRASE and conjunction will use PHRASE as lead! 
                {"value":"test1234"},
                {"value":"test21321312312"},
                {"value":"test213213123122"},
                {"value":"test2132131231222"},
                {"value":"test21321312312222"},
                {"value":"test2132131231222322"},
                {"value":"test2132131231231231222322"},
                // this will make PHRASE iterator not use small_disjunction (more than 5 candidates for LEVENSHTEIN)
                {"value":"rest"},
                {"value":"arest"},
                {"value":"brest"},
                {"value":"zest"},
                {"value":"qest"}];
              db[queryColl].save(documents);
           }
       },
       dropCollections: function() {
         db._drop("AnotherUnitTestsCollection");
         db._drop("AuxUnitTestsCollection");
         db._drop("UnitTestsWithArrayCollection");
         db._drop("TestsCollectionWithManyFields");
         db._drop("TestsCollectionWithLongFields");
         db._drop("DisjunctionCollection");
       },
       getLongValue: function() {
         return longValue;
       }
    },
    testViewInFunctionCall : function () {
      try {
        db._query("FOR doc IN 1..1 RETURN COUNT(UnitTestsView)");
      } catch (e) {
        assertEqual(ERRORS.ERROR_NOT_IMPLEMENTED.code, e.errorNum);
      }
    },

    testV8FunctionInSearch: function () {
      try {
        db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' && V8('abc') == 'ABC' OPTIONS { waitForSync: true } RETURN doc");
        fail();
      } catch (e) {
        assertEqual(ERRORS.ERROR_NOT_IMPLEMENTED.code, e.errorNum);
      }
    },

    testAnalyzerFunctionInReturnStatement: function () {
      var result = db._query("FOR doc IN CompoundView SEARCH doc.name == 'full' OPTIONS { waitForSync: true } RETURN ANALYZER(doc.text, 'text_en')").toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], "the quick brown fox jumps over the lazy dog");
    },

    testViewCollectionOptions : function() {
      var result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } RETURN doc").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });

      result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ @collectionName ] } RETURN doc", { collectionName : 'UnitTestsCollection' }).toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });

      result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : @collections } RETURN doc", { collections : [ 'UnitTestsCollection' ] }).toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });

      result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ " + db.UnitTestsCollection2._id + " ] } RETURN doc").toArray();

      assertEqual(result.length, 5);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection2/'));
      });

      result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ '" + db.UnitTestsCollection2._id + "', 'UnitTestsCollection' ] } RETURN doc").toArray();

      assertEqual(result.length, 15);
      var CountC1 = 0;
      var CountC2 = 0;
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        if (res._id.startsWith('UnitTestsCollection2/')) {
          ++CountC2;
        } else if (res._id.startsWith('UnitTestsCollection/')) {
          ++CountC1;
        }
      });
      assertEqual(CountC1, 10);
      assertEqual(CountC2, 5);

      result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : null } RETURN doc").toArray();

      assertEqual(result.length, 15);
      CountC1 = 0;
      CountC2 = 0;
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        if (res._id.startsWith('UnitTestsCollection2/')) {
          ++CountC2;
        } else if (res._id.startsWith('UnitTestsCollection/')) {
          ++CountC1;
        }
      });
      assertEqual(CountC1, 10);
      assertEqual(CountC2, 5);

      result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : @collections } RETURN doc", { collections: null }).toArray();

      assertEqual(result.length, 15);
      CountC1 = 0;
      CountC2 = 0;
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        if (res._id.startsWith('UnitTestsCollection2/')) {
          ++CountC2;
        } else if (res._id.startsWith('UnitTestsCollection/')) {
          ++CountC1;
        }
      });
      assertEqual(CountC1, 10);
      assertEqual(CountC2, 5);

      result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [] } RETURN doc").toArray();
      assertEqual(result.length, 0);

      result = db._query(
        "FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } FOR doc2 IN CompoundView SEARCH doc2.a == 'foo' OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection2' ] } RETURN { doc, doc2 }"
      ).toArray();

      assertEqual(result.length, 50);
      result.forEach(function(res) {
        assertEqual(res.doc.a, "foo");
        assertEqual(res.doc2.a, "foo");
        assertTrue(res.doc._id.startsWith('UnitTestsCollection/'));
        assertTrue(res.doc2._id.startsWith('UnitTestsCollection2/'));
      });
    },

    testTransactionRegistration : function () {
      // ensure data is synced
      db._query("FOR d IN CompoundView OPTIONS {waitForSync:true} LIMIT 1 RETURN 1");

      // implicit read lock
      var result = db._executeTransaction({
        collections: { },
        action: function () {
          var db = require("@arangodb").db;
          return db._query("FOR d IN CompoundView SEARCH d.name == 'full' RETURN d.text").toArray();
        }
      });
      assertEqual("the quick brown fox jumps over the lazy dog", result[0]);
      assertEqual(1, result.length);

      // read lock
      result = db._executeTransaction({
        collections: {
          allowImplicit: false,
          read: [ "UnitTestsView" ]
        },
        action: function () {
          var db = require("@arangodb").db;
          var c = db._collection("UnitTestsCollection");
          return {length: c.toArray().length, value: c.document('foo').xyz};
        }
      });
      assertEqual(1, result.value);
      assertEqual(28, result.length);

      // write lock
      result = db._executeTransaction({
        collections: {
          allowImplicit: false,
          write: [ "UnitTestsView" ]
        },
        action: function () {
          var db = require("@arangodb").db;
          var c = db._collection("UnitTestsCollection");
          c.save({ _key: "bar", xyz: 2 });
          return c.toArray().length;
        }
      });
      assertEqual(29, result);

      // exclusive lock
      result = db._executeTransaction({
        collections: {
          allowImplicit: false,
          exclusive: [ "UnitTestsView" ]
        },
        action: function () {
          var db = require("@arangodb").db;
          var c = db._collection("UnitTestsCollection");
          c.save({ _key: "baz", xyz: 3 });
          return c.toArray().length;
        }
      });
      assertEqual(30, result);
    },

    testAttributeEqualityFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
      });
    },

    testAttributeEqualityFilterWithWINDOW : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' OPTIONS { waitForSync : true } WINDOW {preceding:'unbounded'} AGGREGATE l = LENGTH(doc) RETURN MERGE(doc, {length:l})").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res, idx) {
        assertEqual(res.a, "foo");
        assertEqual(res.length, idx + 1);
      });
    },

    testMultipleAttributeEqualityFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' && doc.b == 'bar' OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 5);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertEqual(res.b, "bar");
      });
    },

    testMultipleAttributeEqualityFilterSortAttribute : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' && doc.b == 'bar' OPTIONS { waitForSync : true } SORT doc.c RETURN doc").toArray();

      assertEqual(result.length, 5);
      var last = -1;
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertEqual(res.b, "bar");
        assertEqual(res.c, last + 1);
        last = res.c;
      });
    },

    testMultipleAttributeEqualityFilterSortAttributeDesc : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' AND doc.b == 'bar' OPTIONS { waitForSync : true } SORT doc.c DESC RETURN doc").toArray();

      assertEqual(result.length, 5);
      var last = 5;
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertEqual(res.b, "bar");
        assertEqual(res.c, last - 1);
        last = res.c;
      });
    },

    testAttributeLessFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c < 2 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 8);
      result.forEach(function(res) {
        assertTrue(res.c < 2);
      });
    },

    testAttributeLeqFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c <= 2 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c <= 2);
      });
    },

    testAttributeGeqFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c >= 2 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c >= 2);
      });
    },

    testAttributeGreaterFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c > 2 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 8);
      result.forEach(function(res) {
        assertTrue(res.c > 2);
      });
    },

    testAttributeOpenIntervalFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c > 1 AND doc.c < 3 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 4);
      result.forEach(function(res) {
        assertTrue(res.c > 1 && res.c < 3);
      });
    },

    testAttributeClosedIntervalFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c >= 1 AND doc.c <= 3 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c >= 1 && res.c <= 3);
      });
    },

    testAttributeIntervalExclusionFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c < 1 OR doc.c > 3 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 8);
      result.forEach(function(res) {
        assertTrue(res.c < 1 || res.c > 3);
      });
    },

    testAttributeNeqFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a != 'foo' OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 18); // include documents without attribute 'a'
      result.forEach(function(res) {
        assertFalse(res.a === 'foo');
      });
    },

    testStartsWithFilter : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, 'fo') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, 'foo');
      });
    },

    testStartsWithFilter2 : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.b, 'ba') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertTrue(res.b === 'bar' || res.b === 'baz');
      });
    },

    testStartsWithFilterSort : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.b, 'ba') && doc.c == 0 OPTIONS { waitForSync : true } SORT doc.b RETURN doc").toArray();

      assertEqual(result.length, 2);
      assertEqual(result[0].b, 'bar');
      assertEqual(result[1].b, 'baz');
      assertEqual(result[0].c, 0);
      assertEqual(result[1].c, 0);
    },

    testStartsWithFilterArrayWithoutMinMatchCount : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g']) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, 'foo');
      });
    },

    testStartsWithFilterArrayWithoutMinMatchCountViaReference : function () {
      var result = db._query("LET x = NOOPT(['fo', 'g']) FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, x) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, 'foo');
      });
    },

    testStartsWithFilterEmptyArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, []) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 0);
    },

    testStartsWithFilterEmptyArrayMinMatchCountZero : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, [], 0) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 28);
    },

    testStartsWithFilterArrayWithMinMatchCountZero : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g'], 0) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 28);
    },

    testStartsWithFilterArrayWithMinMatchCountTrue : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g'], 1) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, 'foo');
      });
    },

    testStartsWithFilterArrayWithMinMatchCountFalse : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g'], 2) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 0);
    },

    testInTokensFilterSortTFIDF : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(doc.text IN TOKENS('the quick brown', 'text_en'), 'text_en') OPTIONS { waitForSync : true } SORT TFIDF(doc) LIMIT 4 RETURN doc").toArray();
      assertEqual(result.length, 4);
      assertEqual(result[0].name, 'half');
      assertEqual(result[1].name, 'quarter');
      assertEqual(result[2].name, 'other half');
      assertEqual(result[3].name, 'full');
    },

    testWildcardFilter : function() {
      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '_a_' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(2, result.length);
        assertEqual("bar", result[0].a);
        assertEqual("baz", result[1].a);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '_ar' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(1, result.length);
        assertEqual("bar", result[0].a);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '_a__' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(0, result.length);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'ba_' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual("bar", result[0].a);
        assertEqual("baz", result[1].a);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'b__' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(2, result.length);
        assertEqual("bar", result[0].a);
        assertEqual("baz", result[1].a);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'b%' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(2, result.length);
        assertEqual("bar", result[0].a);
        assertEqual("baz", result[1].a);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '%r' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(1, result.length);
        assertEqual("bar", result[0].a);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'b%%' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(2, result.length);
        assertEqual("bar", result[0].a);
        assertEqual("baz", result[1].a);
      }

      {
        let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '%a%' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
        assertEqual(2, result.length);
        assertEqual("bar", result[0].a);
        assertEqual("baz", result[1].a);
      }
    },
    testExistsFilterByAnalyzer : function () {
      var expected = new Set();
      expected.add("full");
      expected.add("half");
      expected.add("other half");
      expected.add("quarter");

      var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text, 'analyzer', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(res.name));
      });
      assertEqual(expected.size, 0);
    },
    testExistsFilter : function () {
      var expected = new Set();
      expected.add("full");
      expected.add("half");
      expected.add("other half");
      expected.add("quarter");

      var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text) OPTIONS { waitForSync : true } RETURN doc").toArray();
      if(!args.oldMangling) {
        assertEqual(result.length, 28);
        return;
      }
      assertEqual(result.length, expected.size); 
      result.forEach(function(res) {
        assertTrue(expected.delete(res.name));
      });
      assertEqual(expected.size, 0);
    },
    testExistsFilterByContextAnalyzer: function () {
      var expected = new Set();
      expected.add("full");
      expected.add("half");
      expected.add("other half");
      expected.add("quarter");

      var result = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(EXISTS(doc.text, 'analyzer'), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(res.name));
      });
      assertEqual(expected.size, 0);
    },

    testExistsFilterByString: function () {
      var expected = new Set();
      expected.add("full");
      expected.add("half");
      expected.add("other half");
      expected.add("quarter");

      var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text, 'string') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(res.name));
      });
      assertEqual(expected.size, 0);
    },
    testExistsFilterByType : function () {
      // FIMXE: decide if it is correct for search-alias
      var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text, 'type') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, args.oldMangling? 0: 28);
    },
    testExistsFilterByTypeNull : function () {
      var expected = new Set();
      expected.add("null");

      var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.anotherNullField, 'null') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(res.name));
      });
      assertEqual(expected.size, 0);
    },

    testExistsFilterByTypeBool : function () {
      var expected = new Set();
      expected.add("bool");

      var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc['anotherBoolField'], 'bool') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(res.name));
      });
      assertEqual(expected.size, 0);
    },

    testExistsFilterByTypeNumeric : function () {
      var expected = new Set();
      expected.add("numeric");

      var result = db._query("LET suffix='NumericField' LET fieldName = CONCAT('another', suffix) FOR doc IN UnitTestsView SEARCH EXISTS(doc[fieldName], 'numeric') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(res.name));
      });
      assertEqual(expected.size, 0);
    },

    testViewInInnerLoop : function() {
      var expected = new Set(); // FIXME is there a better way to compare objects in js?
      expected.add(JSON.stringify({ a: "foo", b: "bar", c: 0 }));
      expected.add(JSON.stringify({ a: "foo", b: "baz", c: 0 }));
      expected.add(JSON.stringify({ a: "bar", b: "foo", c: 1 }));
      expected.add(JSON.stringify({ a: "baz", b: "foo", c: 1 }));

      var result = db._query(
        "FOR adoc IN AnotherUnitTestsCollection " +
        "FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc['a'], adoc.a) && adoc.id == doc.c OPTIONS { waitForSync : true } " +
        "RETURN doc"
      ).toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(JSON.stringify({ a: res.a, b: res.b, c: res.c })));
      });
      assertEqual(expected.size, 0);
    },

    testJoinTwoViewsSortByAttribute : function() {
      var expected = [];
      expected.push({ a: "bar", b: "foo", c: 1 });
      expected.push({ a: "baz", b: "foo", c: 1 });
      expected.push({ a: "foo", b: "bar", c: 1 });
      expected.push({ a: "foo", b: "baz", c: 1 });
      expected.push({ a: "bar", b: "foo", c: 0 });
      expected.push({ a: "baz", b: "foo", c: 0 });
      expected.push({ a: "foo", b: "bar", c: 0 });
      expected.push({ a: "foo", b: "baz", c: 0 });

      var query = "FOR doc0 IN CompoundView OPTIONS { collections: ['UnitTestsCollection2'], waitForSync:true } " +
                  "  FOR doc1 IN UnitTestsView SEARCH doc0.c == doc1.c && STARTS_WITH(doc1['a'], doc0.a) OPTIONS { waitForSync: true } " +
                  "FILTER doc1.c < 2 " +
                  "SORT doc1.c DESC, doc1.a, doc1.b " +
                  "RETURN doc1";

      var viewNodes = getNodes(query, "EnumerateViewNode");
      assertEqual(2, viewNodes.length);

      var viewNode0 = viewNodes[0];
      assertNotUndefined(viewNode0);
      assertEqual(db.UnitTestsCollection2.count(), viewNode0.estimatedNrItems);
      assertEqual(viewNode0.estimatedCost, viewNode0.estimatedNrItems + 1);

      var viewNode1 = viewNodes[1];
      assertNotUndefined(viewNode1);
      assertEqual(db.UnitTestsCollection2.count()*db.UnitTestsCollection.count(), viewNode1.estimatedNrItems);
      assertEqual(viewNode1.estimatedCost, viewNode0.estimatedCost + viewNode1.estimatedNrItems);

      var result = db._query(query, null, { waitForSync: true }).toArray();

      assertEqual(result.length, expected.length);
      var i = 0;
      result.forEach(function(res) {
        var doc = expected[i++];
        assertEqual(doc.a, res.a);
        assertEqual(doc.b, res.b);
        assertEqual(doc.c, res.c);
      });
    },

    testViewInInnerLoopMultipleFilters : function() {
      var expected = new Set(); // FIXME is there a better way to compare objects in js?
      expected.add(JSON.stringify({ a: "foo", b: "bar", c: 0 }));
      expected.add(JSON.stringify({ a: "foo", b: "baz", c: 0 }));

      var result = db._query(
        "FOR adoc IN AnotherUnitTestsCollection FILTER adoc.id < 1 " +
        "FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc['a'], adoc.a) && adoc.id == doc.c OPTIONS { waitForSync : true } " +
        "RETURN doc"
      ).toArray();

      assertEqual(result.length, expected.size);
      result.forEach(function(res) {
        assertTrue(expected.delete(JSON.stringify({ a: res.a, b: res.b, c: res.c })));
      });
      assertEqual(expected.size, 0);
    },

    testViewInInnerLoopSortByAttribute : function() {
      var expected = [];
      expected.push({ a: "bar", b: "foo", c: 1 });
      expected.push({ a: "baz", b: "foo", c: 1 });
      expected.push({ a: "foo", b: "bar", c: 0 });
      expected.push({ a: "foo", b: "baz", c: 0 });

      var result = db._query(
        "FOR adoc IN AnotherUnitTestsCollection " +
        "FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc['a'], adoc.a) && adoc.id == doc.c OPTIONS { waitForSync : true } " +
        "SORT doc.c DESC, doc.a, doc.b " +
        "RETURN doc"
      ).toArray();

      assertEqual(result.length, expected.length);
      var i = 0;
      result.forEach(function(res) {
        var doc = expected[i++];
        assertEqual(doc.a, res.a);
        assertEqual(doc.b, res.b);
        assertEqual(doc.c, res.c);
      });
    },

    testViewInInnerLoopSortByAttributeWithNonDeterministic : function() {
      var expected = [];
      expected.push({ a: "bar", b: "foo", c: 1 });
      expected.push({ a: "baz", b: "foo", c: 1 });
      expected.push({ a: "foo", b: "bar", c: 0 });
      expected.push({ a: "foo", b: "baz", c: 0 });

      var result = db._query(
        "FOR adoc IN AnotherUnitTestsCollection " +
        "FOR doc IN UnitTestsView SEARCH RAND() != -10 && STARTS_WITH(doc['a'], adoc.a) && adoc.id == doc.c OPTIONS { waitForSync : true } " +
        "SORT doc.c DESC, doc.a, doc.b " +
        "RETURN doc"
      ).toArray();

      assertEqual(result.length, expected.length);
      var i = 0;
      result.forEach(function(res) {
        var doc = expected[i++];
        assertEqual(doc.a, res.a);
        assertEqual(doc.b, res.b);
        assertEqual(doc.c, res.c);
      });
    },

    testViewInInnerLoopSortByTFIDF_BM25_Attribute : function() {
      var expected = [];
      expected.push({ a: "baz", b: "foo", c: 1 });
      expected.push({ a: "bar", b: "foo", c: 1 });
      expected.push({ a: "foo", b: "bar", c: 0 });
      expected.push({ a: "foo", b: "baz", c: 0 });

      var result = db._query(
        "FOR adoc IN AnotherUnitTestsCollection " +
        "FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc['a'], adoc.a) && adoc.id == doc.c OPTIONS { waitForSync : true } " +
        "SORT TFIDF(doc) DESC, BM25(doc) DESC, doc.a DESC, doc.b " +
        "RETURN doc"
      ).toArray();

      assertEqual(result.length, expected.length);
      var i = 0;
      result.forEach(function(res) {
        var doc = expected[i++];
        assertEqual(doc.a, res.a);
        assertEqual(doc.b, res.b);
        assertEqual(doc.c, res.c);
      });
    },

    testViewInInnerLoopOptimized : function() {
      var expected = [];
      expected.push({ a: "foo", b: "bar", c: 0 });
      expected.push({ a: "foo", b: "baz", c: 0 });

      var result = db._query("LET outer = (FOR out1 IN UnitTestsCollection FILTER out1.a == 'foo' && out1.c == 0 RETURN out1) FOR a IN outer FOR d IN UnitTestsView SEARCH d.a == a.a && d.c == a.c && d.b == a.b OPTIONS {waitForSync: true} SORT d.b ASC RETURN d").toArray();

      assertEqual(result.length, expected.length);
      var i = 0;
      result.forEach(function(res) {
        var doc = expected[i++];
        assertEqual(doc.a, res.a);
        assertEqual(doc.b, res.b);
        assertEqual(doc.c, res.c);
      });
    },

    
    testAttributeInRangeOpenInterval : function () {
      var query = "FOR doc IN UnitTestsView SEARCH IN_RANGE(doc.c, 1, 3, false, false) OPTIONS { waitForSync : true } RETURN doc";
      var viewNode = getNodes(query, "EnumerateViewNode")[0];
      assertNotUndefined(viewNode);
      assertEqual(db.UnitTestsCollection.count(), viewNode.estimatedNrItems);
      assertEqual(viewNode.estimatedCost, viewNode.estimatedNrItems + 1);

      var result = db._query(query).toArray();
      assertEqual(result.length, 4);
      result.forEach(function(res) {
        assertTrue(res.c > 1 && res.c < 3);
      });
    },

    testAttributeInRangeClosedInterval : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH IN_RANGE(doc.c, 1, 3, true, true) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c >= 1 && res.c <= 3);
      });
    },

    testMutlipleScorers : function() {
      var result = db._query(
        "LET score = FIRST(FOR doc IN UnitTestsView SEARCH doc.a == 'foo' OPTIONS { waitForSync : true } RETURN BM25(doc)) " +
        "FOR doc IN UnitTestsView SEARCH doc.a == 'foo' FILTER BM25(doc) == score && BM25(doc, 1.2, 0.75) == score RETURN doc"
      ).toArray();

      assertEqual(result.length, 10);
      result.forEach(function (res) {
        assertTrue(res.a === "foo");
      });
    },

    testScorersWithDistinct : function() {
      var result = db._query(
        "LET values = (FOR y IN UnitTestsCollection FILTER y.c == 0 RETURN DISTINCT y.a) " +
        "LET x = FIRST(FOR x IN values FILTER x == 'foo' RETURN x) " +
        "FOR doc IN UnitTestsView SEARCH doc.a == x && doc.c == 0 OPTIONS { waitForSync: true } " +
        "LET score = BM25(doc, 20.01, 0.01)*0.0001 + 1.0 " + 
        "COLLECT a = doc.a AGGREGATE maxScore = MAX(score) " +
        "RETURN { a: a, score: maxScore }"
      ).toArray();

      assertEqual(result.length, 1);
      result.forEach(function (res) {
        assertEqual(res.a, "foo");
        assertTrue(res.score > 1 && res.score < 2);
      });
    },
    testAttributeInRange : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c IN 1..3 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c >= 1 || res.c <= 3);
      });
    },
    testAttributeNotInRange : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c NOT IN 1..3 OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 16);
      result.forEach(function(res) {
        assertTrue(res.c === undefined || res.c < 1 || res.c > 3);
      });
    },
    testAttributeInArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c IN [ 1, 3 ] OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 8);
      result.forEach(function(res) {
        assertTrue(res.c === 1 || res.c === 3);
      });
    },
    testAttributeNotInArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c NOT IN [ 1, 3 ] OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 20);
      result.forEach(function(res) {
        assertTrue(res.c === undefined || res.c !== 1 && res.c !== 3);
      });
    },
    testAttributeInExpression : function () {
      var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 IN FLATTEN(c) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, db.UnitTestsCollection.toArray().length);

    },
    testAttributeNotInExpression: function () {
      var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 NOT IN FLATTEN(c) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 0);
    },
    testAttributeInExpressionNonDet : function () {
      var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 IN NOOPT(FLATTEN(c)) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, db.UnitTestsCollection.toArray().length);

    },
    testAttributeNotInExpressionNonDet: function () {
      var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 NOT IN NOOPT(FLATTEN(c)) OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result.length, 0);
    },
    testAnalyzerFunctionPrematureCall : function () {
      assertEqual(
        db._query("FOR d in UnitTestsView SEARCH ANALYZER(d.a IN TOKENS('#', 'text_en'), 'text_en') OPTIONS { waitForSync : true } RETURN d").toArray().length,
        0);
      assertEqual(
        db._query("FOR d in UnitTestsView SEARCH ANALYZER(d.a NOT IN TOKENS('#', 'text_en'), 'text_en') OPTIONS { waitForSync : true } RETURN d").toArray().length,
        28);
    },
    testBoostFunctionPrematureCall : function () {
      assertEqual(
        db._query("FOR d in UnitTestsView SEARCH BOOST(d.a IN TOKENS('#', 'text_en'), 2) OPTIONS { waitForSync : true }  SORT BM25(d) RETURN d").toArray().length,
        0);
      assertEqual(
        db._query("FOR d in UnitTestsView SEARCH BOOST(d.a NOT IN TOKENS('#', 'text_en'), 2) OPTIONS { waitForSync : true }  SORT BM25(d) RETURN d").toArray().length,
        28);
    },
    testMinMatchFunctionPrematureCall : function () {
      assertEqual(
        db._query("FOR d in UnitTestsView SEARCH MIN_MATCH(d.a IN TOKENS('#', 'text_en'), d.a IN TOKENS('#', 'text_de'), 1) OPTIONS { waitForSync : true } RETURN d").toArray().length,
        0);
      assertEqual(
        db._query("FOR d in UnitTestsView SEARCH MIN_MATCH(false, true, true, 2) OPTIONS { waitForSync : true }  RETURN d").toArray().length,
        28);
      assertEqual(
        db._query("FOR d in UnitTestsView SEARCH MIN_MATCH(false, false, false, 0) OPTIONS { waitForSync : true }  RETURN d").toArray().length,
        28);
    },
    
    testArrayComparsionOperatorsInOnSimpleField : function () {
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(15, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a === 'foo' || doc.a === 'bar');
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  [] ANY IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ANY NOT IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  [] ANY NOT IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo'] ANY NOT IN d.a RETURN d").toArray();
        assertEqual(18, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a !== 'foo');
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] NONE IN d.a RETURN d").toArray();
        assertEqual(13, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  [] NONE IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] NONE NOT IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['bar'] NONE NOT IN d.a RETURN d").toArray();
        assertEqual(5, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a === 'bar');
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  [] NONE NOT IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ALL IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  [] ALL IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['bar'] ALL IN d.a RETURN d").toArray();
        assertEqual(5, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a === 'bar');
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ALL NOT IN d.a RETURN d").toArray();
        assertEqual(13, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH  [] ALL NOT IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
    },
    testArrayComparsionOperatorsInOnSimpleFieldWitScopedValue : function () {
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.foobar ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(15, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a === 'foo' || doc.a === 'bar');
        });
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.empty ANY IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.foobar ANY NOT IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.empty ANY NOT IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.foo ANY NOT IN d.a RETURN d").toArray();
        assertEqual(18, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a !== 'foo');
        });
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.foobar NONE IN d.a RETURN d").toArray();
        assertEqual(13, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
        });
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.empty NONE IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.foobar NONE NOT IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.bar NONE NOT IN d.a RETURN d").toArray();
        assertEqual(5, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a === 'bar');
        });
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.empty NONE NOT IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.foobar ALL IN d.a RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.empty ALL IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.bar ALL IN d.a RETURN d").toArray();
        assertEqual(5, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a === 'bar');
        });
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.foobar ALL NOT IN d.a RETURN d").toArray();
        assertEqual(13, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
        });
      }
      {
        let result = db._query("FOR a IN AuxUnitTestsCollection " + 
          "FOR d IN UnitTestsView SEARCH  a.empty ALL NOT IN d.a RETURN d").toArray();
        assertEqual(28, result.length);
      }
    },
    testArrayComparsionOperatorsInOnArrayField : function() {
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['foo', 'bar'] ALL IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['foo', 'bar', 'none'] ALL IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'none', 'nani', 'afoo'] ALL NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['afoo', 'foo', 'none', 'nani'] ALL NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'bar'] ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'nani', 'none'] ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'foo', 'none', 'nani', 'afoo'] ANY NOT IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(2, result.length);
        assertEqual(0, result[0].c);
        assertEqual(1, result[1].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'foo', 'bar'] ANY NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(1, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'nani', 'afoo'] NONE IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'nani', 'bar', 'afoo'] NONE IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'foo', 'bar'] NONE NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo'] NONE NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'boo'] AT LEAST(2) IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'boo'] AT LEAST(2) == d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'boo', 'abaz', 'afoo'] AT LEAST(3) IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(2) IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(2, result.length);
        assertEqual(0, result[0].c);
        assertEqual(1, result[1].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(2) > d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(1, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'afoo'] AT LEAST(2) < d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(2) >= d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(2, result.length);
        assertEqual(0, result[0].c);
        assertEqual(1, result[1].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'afoo'] AT LEAST(2) <= d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(2, result.length);
        assertEqual(0, result[0].c);
        assertEqual(1, result[1].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(3) NOT IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'afoo', 'abar', 'abaz'] AT LEAST(3) NOT IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'afoo', 'abar', 'abaz'] AT LEAST(3) != d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(1, result.length);
        assertEqual(0, result[0].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(2) NOT IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(2, result.length);
        assertEqual(0, result[0].c);
        assertEqual(1, result[1].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(0) IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(2, result.length);
        assertEqual(0, result[0].c);
        assertEqual(1, result[1].c);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(4) IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo', 'abar', 'abaz'] AT LEAST(5) IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
        assertEqual(0, result.length);
      }
    },
    testArrayComparsionOperatorsGreaterOnSimpleField : function() {
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3, 4] ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 0 || doc.c === 1);
        });
      }
      {
        let result = db._query("FOR a IN [[2, 3, 4]] FOR d IN UnitTestsView SEARCH a ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 0 || doc.c === 1);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3, 4] ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(12, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 0 || doc.c === 1 || doc.c === 2);
        });
      }
      {
        let result = db._query("FOR a IN [[2, 3, 4]] FOR d IN UnitTestsView SEARCH a ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(12, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 0 || doc.c === 1 || doc.c === 2);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2] ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c < 2);
        });
      }
      {
        let result = db._query("FOR a IN [[1, 2]] FOR d IN UnitTestsView SEARCH a ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c < 2);
        });

      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2] ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(12, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c <= 2);
        });
      }
      {
        let result = db._query("FOR a IN [[1, 2]] FOR d IN UnitTestsView SEARCH a ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(12, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c <= 2);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4 || doc.c === 3);
        });
      }
      {
        let result = db._query("FOR a IN [[1, 2, 3]] FOR d IN UnitTestsView SEARCH a NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4 || doc.c === 3);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4);
        });
      }
      {
        let result = db._query("FOR a IN [[1, 2, 3]] FOR d IN UnitTestsView SEARCH a NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
    },
    testArrayComparsionOperatorsLessOnSimpleField : function() {
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3] ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4);
        });
      }
      {
        let result = db._query("FOR a IN [[2,3]] FOR d IN UnitTestsView SEARCH a ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3] ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4 || doc.c === 3);
        });
      }
      {
        let result = db._query("FOR a IN [[2,3]] FOR d IN UnitTestsView SEARCH a ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4 || doc.c === 3);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [3, 4] ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4);
        });
      }
      {
        let result = db._query("FOR a IN [[3,4]] FOR d IN UnitTestsView SEARCH a ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [3, 4] ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4 || doc.c === 3);
        });
      }
      {
        let result = db._query("FOR a IN [[3,4]] FOR d IN UnitTestsView SEARCH a ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 4 || doc.c === 3);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(0, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 1 || doc.c === 0);
        });
      }
      {
        let result = db._query("FOR a IN [[1,2,3]] FOR d IN UnitTestsView SEARCH a NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(8, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 1 || doc.c === 0);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue( doc.c === 0);
        });
      }
      {
        let result = db._query("FOR a IN [[1,2,3]] FOR d IN UnitTestsView SEARCH a NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(4, result.length);
        result.forEach(function(doc) {
          assertTrue(doc.c === 0);
        });
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
      {
        let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
        assertEqual(28, result.length);
      }
    },

    testLevenshteinMatch0 : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lazi', 0, false), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(2, res.length);
      assertEqual("full", res[0].name);
      assertEqual("half", res[1].name);
    },

    testLevenshteinMatch1 : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lzi', 1, false), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(2, res.length);
      assertEqual("full", res[0].name);
      assertEqual("half", res[1].name);
    },

    testLevenshteinDamerauMatch1 : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lzai', 1, true), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(2, res.length);
      assertEqual("full", res[0].name);
      assertEqual("half", res[1].name);
    },

    testLevenshteinDamerauMatchPrefix : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'zi', 1, true, 64, 'la'), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(2, res.length);
      assertEqual("full", res[0].name);
      assertEqual("half", res[1].name);
    },

    testLevenshteinDamerauMatch1Default : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lzai', 1), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(2, res.length);
      assertEqual("full", res[0].name);
      assertEqual("half", res[1].name);
    },

    testLevenshteinDamerauMatch1NoLimit : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lzai', 1, true), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(2, res.length);
      assertEqual("full", res[0].name);
      assertEqual("half", res[1].name);
    },

    testLevenshteinMatch2 : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'dog', 2, false), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(3, res.length);
      assertEqual("full", res[0].name);
      assertEqual("half", res[1].name);
      assertEqual("other half", res[2].name);
    },

    testLevenshteinMatch2Limit1 : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'dog', 2, false, 1), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
      assertEqual(2, res.length);
      assertEqual("full", res[0].name);
      assertEqual("other half", res[1].name);
    },

    testLevenshteinMatch2BM25 : function() {
      var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'dog', 2, false), 'text_en') OPTIONS { waitForSync : true } SORT BM25(doc) DESC LIMIT 1 RETURN doc").toArray();
      assertEqual(1, res.length);
      assertEqual("other half", res[0].name);
    },

    testAnalyzerNotOrFalse : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(doc.a != 'foo' OR FALSE, 'identity') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertNotEqual(0, result.length);
    },

    testPhraseTerm : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {TERM: 'quick'}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
      
    },
    
    testPhraseTermViaVariable : function () {
      var result = db._query("LET p = NOOPT({TERM: 'quick'}) "
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
      
    },

    testPhraseTermViaArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{TERM: 'quick'}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseTermViaArrayVariable : function() {
      var result = db._query("LET p = NOOPT([{TERM: 'quick'}, 0, 'brown']) "
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseStartsWith : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {STARTS_WITH: 'qui'}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseStartsWithViaVariable : function () {
      var result = db._query("LET p = NOOPT({STARTS_WITH: 'qui'}) "
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseStartsWithViaArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{STARTS_WITH: 'qui'}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseStartsWithViaArrayVariable : function () {
      var result = db._query("LET p = NOOPT([{STARTS_WITH: 'qui'}, 'brown']) "
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseWildcard : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {WILDCARD: 'qu_ck'}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseWildcardViaVariable : function () {
      var result = db._query("LET p = NOOPT({WILDCARD: 'qu_ck'}) "
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseWildcardViaArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{WILDCARD: 'qu_ck'}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseWildcardViaArrayVariable : function () {
      var result = db._query("LET p = NOOPT([{WILDCARD: 'qu_ck'}, 'brown'])"
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseLevenshteinMatch : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {LEVENSHTEIN_MATCH: ['queck', 1, false]}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseLevenshteinMatchViaVariable : function () {
      var result = db._query("LET p = {LEVENSHTEIN_MATCH: ['queck', 1, false]}"
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseLevenshteinMatchNoLimit : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {LEVENSHTEIN_MATCH: ['queck', 1, false, 0]}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseLevenshteinMatchViaArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{LEVENSHTEIN_MATCH: ['queck', 1, false]}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseLevenshteinMatchViaArrayVariable : function () {
      var result = db._query("LET p = NOOPT([{LEVENSHTEIN_MATCH: ['queck', 1, false]}, 'brown']) "
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseTerms : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {TERMS: ['quick', 'fast']}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseTermsViaVariable : function () {
      var result = db._query("LET p = NOOPT({TERMS: ['quick', 'fast']})"
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseTermsViaArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{TERMS: ['quick', 'fast']}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseTermsViaArrayVariable : function () {
      var result = db._query("LET p = NOOPT([{TERMS: ['quick', 'fast']}, 'brown'])"
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseInRange : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {IN_RANGE: ['quic', 'ruick', false, true]}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseInRangeViaVariable : function () {
      var result = db._query("LET p = NOOPT({IN_RANGE: ['quic', 'ruick', false, true]})"
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },

    testPhraseInRangeViaArray : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{IN_RANGE: ['quic', 'ruick', false, true]}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testPhraseInRangeViaArrayVariable : function () {
      var result = db._query("LET p = NOOPT([{IN_RANGE: ['quic', 'ruick', false, true]}, 'brown'])"
                             + "FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, p, 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testVolatileFilter : function() {
      let result = db._query("FOR doc IN AnotherUnitTestsCollection LET kk = NOEVAL(doc.id) "
                             + " FOR c IN UnitTestsWithArrayView SEARCH c.c == kk "
                             + " OPTIONS { waitForSync : true } SORT c.c RETURN c ").toArray();
      assertEqual(2, result.length);
      assertEqual(result[0].c, 0);
      assertEqual(result[1].c, 1);
    },
    
    testQueryWithMultipleSortView : function() {
      let res= db._query("FOR doc IN WithPrimarySort SEARCH ANALYZER(doc.field3 == 1, 'customAnalyzer') "
      + " OPTIONS { waitForSync : true } SORT doc._key  LIMIT 0, 50  RETURN doc ").toArray();
      assertEqual(1, res.length);
    },
    testReadPrimarySortStoredValues : function() {
      var result = db._query("FOR doc IN WithLongPrimarySort SEARCH doc.field3 > 0 OPTIONS { waitForSync : true } SORT doc.field1 ASC " +
                             " RETURN {f1:doc.field1, f2: doc.field2, f3: doc.field3}").toArray();
      assertEqual(1499, result.length);
      let expected = [];
      for (let k = 1; k < 1500; ++k) {
        expected.push(longValue + k);
      }
      expected.sort();
      for (let k = 0; k < 1499; ++k) {
        assertEqual(expected[k], result[k].f1);
      }
    },
    testReadStoredValues : function() {
      var result = db._query("FOR doc IN WithStoredValues SEARCH doc.field3 > 0 OPTIONS { waitForSync : true } SORT doc.field3 ASC " +
                             " RETURN {f1:doc.field1, f2: doc.field2, f3: doc.field3}").toArray();
      assertEqual(1499, result.length);
      for (let k = 0; k < 1499; ++k) {
        assertEqual(k + 1, result[k].f3);
        assertEqual(longValue + (k + 1), result[k].f1);
        assertEqual(longValue, result[k].f2);
      }
    },
    testDisjunctionVisit : function() {
      if (!args.oldMangling) {
        // this test require multiple analyzers per field
        return;
      }
      let res = db._query("FOR d IN DisjunctionView SEARCH "
                  + " (ANALYZER(PHRASE(d.value, {LEVENSHTEIN_MATCH : ['test', 2, true]}), 'text_en') "
                  + " && ANALYZER(STARTS_WITH(d.value, 'test'),'text_en')) OR "
                  + " BOOST(PHRASE(d.value, 'test144', 'identity'), 10) "
                  + " OPTIONS {waitForSync:true} SORT BM25(d) DESC LIMIT 20 "
                  + " RETURN {'Score':BM25(d), 'Id' : d.value, 'Entity' : d }").toArray();
      assertEqual(1, res.length);
    }
  };
};
}());
