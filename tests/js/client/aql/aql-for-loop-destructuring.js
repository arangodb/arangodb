/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const cn = "UnitTestsCollection";

function destructuringSuite () {
  const expectError = (query, msg) => {
    try {
      db._query(query);
      fail();
    } catch (err) {
      assertEqual(errors.ERROR_QUERY_PARSE.code, err.errorNum);
      assertTrue(err.errorMessage.includes(msg), { err, msg });
    }
  };

  return {
    testReassigning : function ()  {
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `FOR {a, a} IN [] RETURN a`);
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `FOR [a, b, c, d, c] IN [] RETURN a`);
    },

    testObjectDestructuringOnNonObjectSource : function ()  {
      [ 
        null,
        false,
        true,
        -1,
        1045,
        "",
        " ",
        "foobar",
        [1, 2, 3],
      ].forEach(function(value) {
        let query = `FOR {foo, bar} IN [${JSON.stringify(value)}] RETURN {foo, bar}`;
        let res = db._query(query).toArray();
        assertEqual({ foo: null, bar: null }, res[0]);
      });
    },
    
    testObjectDestructuringPartiallySet : function ()  {
      [ 
        [{}, {foo: null, bar: null}],
        [{foo: 1, baz: 3}, {foo: 1, bar: null}],
        [{boo: 2, bar: 4}, {foo: null, bar: 4}],
        [{noo: 3, boo: 3}, {foo: null, bar: null}],
      ].forEach(function([value, expected]) {
        let query = `FOR {foo, bar} IN [${JSON.stringify(value)}] RETURN {foo, bar}`;
        let res = db._query(query).toArray();
        assertEqual(expected, res[0]);
      });
    },
    
    testObjectDestructuringSkipping : function ()  {
      [ 
        [{foo: 1, baz: 3}, {foo: 1, bar: null}],
        [{boo: 2, bar: 4}, {foo: null, bar: 4}],
        [{noo: 3, boo: 3}, {foo: null, bar: null}],
      ].forEach(function([value, expected]) {
        let query = `FOR {, foo, bar} IN [${JSON.stringify(value)}] RETURN {foo, bar}`;
        let res = db._query(query).toArray();
        assertEqual(expected, res[0]);
      });
    },
    
    testArrayDestructuringOnNonArraySource : function ()  {
      [ 
        null,
        false,
        true,
        -1,
        1045,
        "",
        " ",
        "foobar",
        { foo: 1, bar: 2 },
      ].forEach(function(value) {
        let query = `FOR [foo, bar] IN [${JSON.stringify(value)}] RETURN {foo, bar}`;
        let res = db._query(query).toArray();
        assertEqual({ foo: null, bar: null }, res[0]);
      });
    },
    
    testArrayDestructuringPartiallySet : function ()  {
      [ 
        [[], {foo: null, bar: null}],
        [[1], {foo: 1, bar: null}],
        [[null, 1], {foo: null, bar: 1}],
        [[1, 2, 3], {foo: 1, bar: 2}],
      ].forEach(function([value, expected]) {
        let query = `FOR [foo, bar] IN [${JSON.stringify(value)}] RETURN {foo, bar}`;
        let res = db._query(query).toArray();
        assertEqual(expected, res[0]);
      });
    },
    
    testArrayDestructuringSkipping : function ()  {
      [ 
        [[], {foo: null, bar: null}],
        [[1], {foo: null, bar: null}],
        [[null, 1], {foo: 1, bar: null}],
        [[1, 2, 3], {foo: 2, bar: 3}],
      ].forEach(function([value, expected]) {
        let query = `FOR [, foo, bar] IN [${JSON.stringify(value)}] RETURN {foo, bar}`;
        let res = db._query(query).toArray();
        assertEqual(expected, res[0]);
      });
    },
    
    testDestructuringDocumentAttributes : function () {
      let c = db._create(cn);
      try {
        let docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({ value1: i, value2: "test" + i, value3: "unused" });
        }
        c.insert(docs);
        
        let query = `FOR {value1, value2} IN ${cn} RETURN {value1, value2}`;
        let res = db._query(query).toArray();
        assertEqual(100, res.length);
        res.forEach((r, i) => {
          assertEqual(r.value1, i);
          assertEqual(r.value2, "test" + i);
        });
      } finally {
        db._drop(cn);
      }
    },
    
    testDestructuringNestedDocumentAttributes : function () {
      let c = db._create(cn);
      try {
        let docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({ value1: {sub1: i, sub2: "test" + i}, value2: "test" + i });
        }
        c.insert(docs);
        
        let query = `FOR {value1: {sub1, sub2}, value2} IN ${cn} RETURN {sub1, sub2, value2}`;
        let res = db._query(query).toArray();
        assertEqual(100, res.length);
        res.forEach((r, i) => {
          assertEqual(r.sub1, i);
          assertEqual(r.sub2, "test" + i);
          assertEqual(r.value2, "test" + i);
        });
      } finally {
        db._drop(cn);
      }
    },
    
    testDestructuringNestedArrayAttributes : function () {
      let c = db._create(cn);
      try {
        let docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({ value1: [i, "test" + i], value2: [i, i + 1] });
        }
        c.insert(docs);
        
        let query = `FOR {value1: [sub1, sub2], value2: [first, second]} IN ${cn} RETURN {sub1, sub2, first, second}`;
        let res = db._query(query).toArray();
        assertEqual(100, res.length);
        res.forEach((r, i) => {
          assertEqual(r.sub1, i);
          assertEqual(r.sub2, "test" + i);
          assertEqual(r.first, i);
          assertEqual(r.second, i + 1);
        });
      } finally {
        db._drop(cn);
      }
    },
    
    testDestructuringArrayValues : function () {
      let query = `LET array = (FOR i IN 0..9 RETURN [i, i + 1, i + 2]) FOR [first, second, third, fourth] IN array RETURN {first, second, third, fourth}`;
      let res = db._query(query).toArray();
      assertEqual(10, res.length);
      res.forEach((r, i) => {
        assertEqual(r.first, i);
        assertEqual(r.second, i + 1);
        assertEqual(r.third, i + 2);
      });
    },
    
    testDestructuringForExpression : function () {
      let c = db._create(cn);
      try {
        let docs = [];
        for (let i = 0; i < 3; ++i) {
          docs.push({ value1: "test" + i, value2: i, value3: "testi" + i });
        }
        c.insert(docs);
        
        let query = `FOR doc IN ${cn} FOR [key, value] IN ENTRIES(doc) FILTER key =~ '^value' SORT key, value RETURN {key, value}`;
        let res = db._query(query).toArray();

        const expected = [
          {"key" : "value1", "value" : "test0"},
          {"key" : "value1", "value" : "test1"},
          {"key" : "value1", "value" : "test2"},
          {"key" : "value2", "value" : 0},
          {"key" : "value2", "value" : 1},
          {"key" : "value2", "value" : 2},
          {"key" : "value3", "value" : "testi0"},
          {"key" : "value3", "value" : "testi1"},
          {"key" : "value3", "value" : "testi2"},
        ];

        assertEqual(res, expected);
      } finally {
        db._drop(cn);
      }
    },

    testDestructuringForView : function () {
      let c = db._create(cn);
      try {
        let docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({ value1: i, value2: "test" + i });
        }
        c.insert(docs);
        db._createView(cn + "View", "arangosearch", {});
        db._view(cn + "View").properties({ links: { [cn]: { includeAllFields: true } } });
        
        let query = `FOR {value1, value2} IN ${cn + "View"} OPTIONS { waitForSync: true } RETURN {value1, value2}`;
        let res = db._query(query).toArray();
        assertEqual(100, res.length);
        res.forEach((r, i) => {
          assertEqual(r.value1, i);
          assertEqual(r.value2, "test" + i);
        });
      } finally {
        db._dropView(cn + "View");
        db._drop(cn);
      }
    },
    
    testDestructuringForViewWithSearchClause : function () {
      let c = db._create(cn);
      try {
        db._createView(cn + "View", "arangosearch", {});
        db._view(cn + "View").properties({ links: { [cn]: { includeAllFields: true } } });
        
        let query = `FOR {value1, value2} IN ${cn + "View"} SEARCH value1 == 1 RETURN {value1, value2}`;
        expectError(query, "SEARCH condition is incompatible with destructuring");
        
        query = `FOR {value1, value2} IN ${cn + "View"} SEARCH value1 == 1 OPTIONS { waitForSync: true } RETURN {value1, value2}`;
        expectError(query, "SEARCH condition is incompatible with destructuring");
      } finally {
        db._dropView(cn + "View");
        db._drop(cn);
      }
    },
    
    testDestructuringForTraversal : function () {
      let c = db._create(cn);
      try {
        db._createEdgeCollection(cn + "Edge");
        
        let query = `WITH ${cn} FOR [v, e, p] IN 1..1 OUTBOUND '${cn}/test0' ${cn}Edge RETURN v`;
        expectError(query, "Traversal does not support array/object destructuring");
        
        query = `WITH ${cn} FOR {v, e, p} IN 1..1 OUTBOUND '${cn}/test0' ${cn}Edge RETURN v`;
        expectError(query, "Traversal does not support array/object destructuring");
      } finally {
        db._drop(cn + "Edge");
        db._drop(cn);
      }
    },
    
    testDestructuringForShortestPath : function () {
      let c = db._create(cn);
      try {
        db._createEdgeCollection(cn + "Edge");
        
        let query = `WITH ${cn} FOR [s] IN OUTBOUND SHORTEST_PATH '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "SHORTEST_PATH does not support array/object destructuring");
        
        query = `WITH ${cn} FOR {s} IN OUTBOUND SHORTEST_PATH '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "SHORTEST_PATH does not support array/object destructuring");
      } finally {
        db._drop(cn + "Edge");
        db._drop(cn);
      }
    },
    
    testDestructuringForAllShortestPaths : function () {
      let c = db._create(cn);
      try {
        db._createEdgeCollection(cn + "Edge");
        
        let query = `WITH ${cn} FOR [s] IN OUTBOUND ALL_SHORTEST_PATHS '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "ALL_SHORTEST_PATHS does not support array/object destructuring");
        
        query = `WITH ${cn} FOR {s} IN OUTBOUND ALL_SHORTEST_PATHS '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "ALL_SHORTEST_PATHS does not support array/object destructuring");
      } finally {
        db._drop(cn + "Edge");
        db._drop(cn);
      }
    },
    
    testDestructuringForKShortestPaths : function () {
      let c = db._create(cn);
      try {
        db._createEdgeCollection(cn + "Edge");
        
        let query = `WITH ${cn} FOR [s] IN OUTBOUND K_SHORTEST_PATHS '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "K_SHORTEST_PATHS does not support array/object destructuring");
        
        query = `WITH ${cn} FOR {s} IN OUTBOUND K_SHORTEST_PATHS '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "K_SHORTEST_PATHS does not support array/object destructuring");
      } finally {
        db._drop(cn + "Edge");
        db._drop(cn);
      }
    },

    testDestructuringForKPaths : function () {
      let c = db._create(cn);
      try {
        db._createEdgeCollection(cn + "Edge");
        
        let query = `WITH ${cn} FOR [s] IN OUTBOUND K_PATHS '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "K_PATHS does not support array/object destructuring");
        
        query = `WITH ${cn} FOR {s} IN OUTBOUND K_PATHS '${cn}/test0' TO '${cn}/test1' ${cn}Edge RETURN s`;
        expectError(query, "K_PATHS does not support array/object destructuring");
      } finally {
        db._drop(cn + "Edge");
        db._drop(cn);
      }
    },

  };
}

jsunity.run(destructuringSuite);
return jsunity.done();
