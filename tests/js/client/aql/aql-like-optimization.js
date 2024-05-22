/* jshint globalstrict:false, strict:false, maxlen:5000 */
/* global assertEqual, assertNotEqual, assertTrue, assertMatch */

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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const db = require('@arangodb').db;

function likeOptimizationSuite () {
  const cn = "UnitTestsLike";
      
  const seqs = [
    "a", 
    "b", 
    "c", 
    "aA", 
    "ab", 
    "abc", 
    "A", 
    "AA", 
    "AB", 
    "Aab", 
    "ABb", 
    "ABC", 
    "ABc", 
    "Aa", 
    "Ab", 
    "Ac", 
    "Abc", 
    "B", 
    "BA", 
    "Ba", 
    "ba", 
    "bA", 
    "bac", 
    "BaC", 
    "bAC", 
    "C", 
  ];

  return {
    setUpAll: function () {
      let c = db._create(cn, { numberOfShards: 1 });
      let docs = [];
      seqs.forEach((v) => {
        docs.push({ value1: v, value2: v });
      });

      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value1"], name: "per" });
    },

    tearDownAll: function () {
      db._drop(cn);
    },
    
    testDoNotOptimizeForInvertedIndex: function () {
      let idx = db[cn].ensureIndex({ type: "inverted", fields: ["name"], name: "inv" });

      const queries = [
        `FOR doc IN ${cn} OPTIONS { indexHint: 'inv', forceIndexHint: true } FILTER LIKE(doc.name, 'a%') RETURN doc`,
        `FOR doc IN ${cn} OPTIONS { indexHint: 'inv', forceIndexHint: true } FILTER LIKE(doc.name, 'a') RETURN doc`,
        `FOR doc IN ${cn} OPTIONS { indexHint: 'inv', forceIndexHint: false } FILTER LIKE(doc.name, 'a%') RETURN doc`,
        `FOR doc IN ${cn} OPTIONS { indexHint: 'inv', forceIndexHint: false } FILTER LIKE(doc.name, 'a') RETURN doc`,
      ];
      
      try {
        queries.forEach((q) => {
          let plan = db._createStatement(q).explain().plan;
          let nodes = plan.nodes.filter((n) => n.type === 'EnumerateCollectionNode');
          assertEqual(0, nodes.length);
          nodes = plan.nodes.filter((n) => n.type === 'CalculationNode');
          assertEqual(0, nodes.length);
          nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
          assertEqual(1, nodes.length);
          let calc = nodes[0].condition;
          assertEqual("n-ary or", calc.type);
          let sub = calc.subNodes;
          assertEqual(1, sub.length);
          assertEqual("n-ary and", sub[0].type);
          sub = sub[0].subNodes;
          assertEqual(1, sub.length);
          assertEqual("function call", sub[0].type);
          assertEqual("LIKE", sub[0].name);
          sub = sub[0].subNodes;
          assertEqual(1, sub.length);
          assertEqual("array", sub[0].type);
          sub = sub[0].subNodes;
          assertEqual(2, sub.length);
          assertEqual("attribute access", sub[0].type);
          assertEqual("value", sub[1].type);
        });
      } finally {
        db[cn].dropIndex(idx);
      }
    },

    testDoNotOptimizeForView: function () {
      const queries = [
        `FOR doc IN ${cn}View SEARCH LIKE(doc.name, 'a%') RETURN doc`,
        `FOR doc IN ${cn}View SEARCH LIKE(doc.name, 'a') RETURN doc`,
      ];
      
      let v = db._createView(cn + "View", "arangosearch", {});
      
      try {
        queries.forEach((q) => {
          let plan = db._createStatement(q).explain().plan;
          let nodes = plan.nodes.filter((n) => n.type === 'EnumerateViewNode');
          assertEqual(1, nodes.length);
          let calc = nodes[0].condition;
          assertEqual("n-ary or", calc.type);
          let sub = calc.subNodes;
          assertEqual(1, sub.length);
          assertEqual("n-ary and", sub[0].type);
          sub = sub[0].subNodes;
          assertEqual(1, sub.length);
          assertEqual("function call", sub[0].type);
          assertEqual("LIKE", sub[0].name);
          sub = sub[0].subNodes;
          assertEqual(1, sub.length);
          assertEqual("array", sub[0].type);
          sub = sub[0].subNodes;
          assertEqual(2, sub.length);
          assertEqual("attribute access", sub[0].type);
          assertEqual("value", sub[1].type);
        });
      } finally {
        db._dropView(cn + "View");
      }
    },
      
    testLikeOnNonAttributeAccess: function () {
      // NOOPT => we should see a normal, unmodified LIKE in the query
      const queries = [
        [ `RETURN NOOPT(LIKE('a', 'a'))`, true ],
        [ `RETURN NOOPT(LIKE('b', 'b'))`, true ],
        [ `RETURN NOOPT(LIKE('a', 'A'))`, false ],
        [ `RETURN NOOPT(LIKE('A', 'a'))`, false ],
        [ `RETURN NOOPT(LIKE('A', 'B'))`, false ],
        [ `RETURN NOOPT(LIKE('AA', 'AA'))`, true ],
        [ `RETURN NOOPT(LIKE('Aa', 'Aa'))`, true ],
        [ `RETURN NOOPT(LIKE('aa', 'aa'))`, true ],
        [ `RETURN NOOPT(LIKE('aA', 'aA'))`, true ],
        [ `RETURN NOOPT(LIKE('ab', 'aa'))`, false ],
        [ `RETURN NOOPT(LIKE('b', 'b'))`, true ],
        [ `RETURN NOOPT(LIKE('b', 'B'))`, false ],
        [ `RETURN NOOPT(LIKE('b', 'c'))`, false ],
        [ `RETURN NOOPT(LIKE('Aa', 'A%'))`, true ],
        [ `RETURN NOOPT(LIKE('AA', 'A%'))`, true ],
        [ `RETURN NOOPT(LIKE('Ab', 'A%'))`, true ],
        [ `RETURN NOOPT(LIKE('ab', 'A%'))`, false ],
        [ `RETURN NOOPT(LIKE('ab', 'AB%'))`, false ],
        [ `RETURN NOOPT(LIKE('abc', 'AB%'))`, false ],
        [ `RETURN NOOPT(LIKE('AB', 'AB%'))`, true ],
        [ `RETURN NOOPT(LIKE('Ab', 'AB%'))`, false ],
        [ `RETURN NOOPT(LIKE('ABc', 'AB%'))`, true ],
        [ `RETURN NOOPT(LIKE('Abc', 'AB%'))`, false ],
      ];

      queries.forEach((q) => {
        let plan = db._createStatement(q[0]).explain().plan;
        let nodes = plan.nodes.filter((n) => n.type === 'CalculationNode');
        assertEqual(1, nodes.length);
        let calc = nodes[0].expression;
        assertEqual("function call", calc.type);
        assertEqual("NOOPT", calc.name);
        let sub = calc.subNodes;
        assertEqual(1, sub.length);
        assertEqual("array", sub[0].type);
        sub = sub[0].subNodes;
        assertEqual(1, sub.length);
        assertEqual("function call", sub[0].type);
        assertEqual("LIKE", sub[0].name);
        sub = sub[0].subNodes;
        assertEqual(1, sub.length);
        assertEqual("array", sub[0].type);
        assertEqual(2, sub[0].raw.length);

        let res = db._query(q[0]).toArray();
        assertEqual(res[0], q[1]);
      });
    },
    
    testLikeNoWildcardNonIndexed: function () {
      const queries = [
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'a') RETURN doc.value2`, ["a"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'b') RETURN doc.value2`, ["b"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'A') RETURN doc.value2`, ["A"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'Ab') RETURN doc.value2`, ["Ab"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'AA') RETURN doc.value2`, ["AA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'aA') RETURN doc.value2`, ["aA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'ab') RETURN doc.value2`, ["ab"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, 'C') RETURN doc.value2`, ["C"] ],
      ];

      queries.forEach((q) => {
        let plan = db._createStatement(q[0]).explain().plan;
        let nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'CalculationNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'EnumerateCollectionNode');
        assertEqual(1, nodes.length);
        let calc = nodes[0].filter;

        assertEqual("compare ==", calc.type);
        let sub = calc.subNodes;
        assertEqual(2, sub.length);
        assertEqual("attribute access", sub[0].type);
        assertEqual("value2", sub[0].name);
        assertEqual("doc", sub[0].subNodes[0].name);
        assertEqual("value", sub[1].type);

        let res = db._query(q[0]).toArray();
        assertEqual(res, q[1]);
      });
    },
    
    testLikeNoWildcardIndexed: function () {
      const queries = [
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'a') RETURN doc.value1`, ["a"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'b') RETURN doc.value1`, ["b"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'A') RETURN doc.value1`, ["A"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'Ab') RETURN doc.value1`, ["Ab"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'AA') RETURN doc.value1`, ["AA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'aA') RETURN doc.value1`, ["aA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'ab') RETURN doc.value1`, ["ab"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'C') RETURN doc.value1`, ["C"] ],
        
        [`FOR doc IN ${cn} OPTIONS { indexHint: 'per', forceIndexHint: true } FILTER LIKE(doc.value1, 'a') RETURN doc.value1`, ["a"] ],
        [`FOR doc IN ${cn} OPTIONS { indexHint: 'per', forceIndexHint: false } FILTER LIKE(doc.value1, 'a') RETURN doc.value1`, ["a"] ],
      ];

      queries.forEach((q) => {
        let plan = db._createStatement(q[0]).explain().plan;
        let nodes = plan.nodes.filter((n) => n.type === 'EnumerateCollectionNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'CalculationNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
        assertEqual(1, nodes.length);
        let calc = nodes[0].condition;
        assertEqual("n-ary or", calc.type);
        let sub = calc.subNodes;
        assertEqual(1, sub.length);
        assertEqual("n-ary and", sub[0].type);
        sub = sub[0].subNodes;
        assertEqual(1, sub.length);
        assertEqual("compare ==", sub[0].type);
        sub = sub[0].subNodes;
        assertEqual(2, sub.length);
        assertEqual("attribute access", sub[0].type);
        assertEqual("value1", sub[0].name);
        assertEqual("doc", sub[0].subNodes[0].name);
        assertEqual("value", sub[1].type);

        let res = db._query(q[0]).toArray();
        assertEqual(res, q[1]);
      });
    },

    testLikeWithNonSuffixWildcard: function () {
      const queries = [
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%a') SORT doc.value2 RETURN doc.value2`, ["a", "Aa", "Ba", "ba"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%b') SORT doc.value2 RETURN doc.value2`, ["Aab", "Ab", "ab", "ABb", "b"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%A') SORT doc.value2 RETURN doc.value2`, ["A", "AA", "aA", "BA", "bA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%Ab') SORT doc.value2 RETURN doc.value2`, ["Ab"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%AA') SORT doc.value2 RETURN doc.value2`, ["AA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%aA') SORT doc.value2 RETURN doc.value2`, ["aA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%ab') SORT doc.value2 RETURN doc.value2`, ["Aab", "ab"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%C') SORT doc.value2 RETURN doc.value2`, ["ABC", "BaC", "bAC", "C"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%B%a') SORT doc.value2 RETURN doc.value2`, ["Ba"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value2, '%a%b%c') SORT doc.value2 RETURN doc.value2`, ["abc"] ],
      ];

      queries.forEach((q) => {
        let plan = db._createStatement(q[0]).explain().plan;
        let nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'CalculationNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'EnumerateCollectionNode');
        assertEqual(1, nodes.length);
        let calc = nodes[0].filter;
        
        assertEqual("function call", calc.type);
        assertEqual("LIKE", calc.name);
        let sub = calc.subNodes;
        assertEqual(1, sub.length);
        assertEqual("array", sub[0].type);
        assertEqual(2, sub[0].subNodes.length);
        assertEqual("attribute access", sub[0].subNodes[0].type);
        assertEqual("doc", sub[0].subNodes[0].subNodes[0].name);
        assertEqual("value", sub[0].subNodes[1].type);
        
        let res = db._query(q[0]).toArray();
        assertEqual(res, q[1]);
      });
    },
    
    testLikeWithSuffixWildcardIndexed: function () {
      const queries = [
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'a%') SORT doc.value1 RETURN doc.value1`, ["a", "aA", "ab", "abc"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'b%') SORT doc.value1 RETURN doc.value1`, ["b", "bA", "ba", "bAC", "bac"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'A%') SORT doc.value1 RETURN doc.value1`, ["A", "AA", "Aa", "Aab", "AB", "Ab", "ABb", "ABC", "ABc", "Abc", "Ac"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'Ab%') SORT doc.value1 RETURN doc.value1`, ["Ab", "Abc"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'AA%') SORT doc.value1 RETURN doc.value1`, ["AA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'aA%') SORT doc.value1 RETURN doc.value1`, ["aA"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'ab%') SORT doc.value1 RETURN doc.value1`, ["ab", "abc"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'C%') SORT doc.value1 RETURN doc.value1`, ["C"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'B%a%') SORT doc.value1 RETURN doc.value1`, ["Ba", "BaC"] ],
        [`FOR doc IN ${cn} FILTER LIKE(doc.value1, 'a%b%c%') SORT doc.value1 RETURN doc.value1`, ["abc"] ],
        
        [`FOR doc IN ${cn} OPTIONS { indexHint: 'per', forceIndexHint: true } FILTER LIKE(doc.value1, 'a%') SORT doc.value1 RETURN doc.value1`, ["a", "aA", "ab", "abc"] ],
      ];

      queries.forEach((q) => {
        let plan = db._createStatement(q[0]).explain().plan;
        let nodes = plan.nodes.filter((n) => n.type === 'EnumerateCollectionNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'CalculationNode');
        assertEqual(0, nodes.length);
        nodes = plan.nodes.filter((n) => n.type === 'IndexNode');
        assertEqual(1, nodes.length);
        let calc = nodes[0].condition;
        assertEqual("n-ary or", calc.type);
        let sub = calc.subNodes;
        assertEqual(1, sub.length);
        assertEqual("n-ary and", sub[0].type);
        sub = sub[0].subNodes;
        assertEqual(2, sub.length);
        if (sub[0].type !== "compare >=") {
          // swap sides
          let tmp = sub[1];
          sub[1] = sub[0];
          sub[0] = tmp;
        }
        assertEqual("compare >=", sub[0].type);
        assertEqual(2, sub[0].subNodes.length);
        assertEqual("attribute access", sub[0].subNodes[0].type);
        assertEqual("doc", sub[0].subNodes[0].subNodes[0].name);
        assertEqual("value", sub[0].subNodes[1].type);

        assertEqual("compare <", sub[1].type);
        assertEqual(2, sub[1].subNodes.length);
        assertEqual("attribute access", sub[1].subNodes[0].type);
        assertEqual("doc", sub[1].subNodes[0].subNodes[0].name);
        assertEqual("value", sub[1].subNodes[1].type);
        
        calc = nodes[0].filter;
        assertEqual("function call", calc.type);
        assertEqual("LIKE", calc.name);
        sub = calc.subNodes;
        assertEqual(1, sub.length);
        assertEqual("array", sub[0].type);
        assertEqual(2, sub[0].subNodes.length);
        assertEqual("attribute access", sub[0].subNodes[0].type);
        assertEqual("doc", sub[0].subNodes[0].subNodes[0].name);
        assertEqual("value", sub[0].subNodes[1].type);
        
        let res = db._query(q[0]).toArray();
        assertEqual(res, q[1]);
      });
    },

  };
}

jsunity.run(likeOptimizationSuite);
return jsunity.done();
