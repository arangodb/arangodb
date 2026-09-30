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
// //////////////////////////////////////////////////////////////////////////////

"use strict";

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const aql = arangodb.aql;
const { assertTrue, assertFalse, assertEqual, assertNotEqual } =
  jsunity.jsUnity.assertions;
const _ = require("lodash");

const useIndexes = "use-indexes";
const removeFilterCoveredByIndex = "remove-filter-covered-by-index";
const moveFiltersIntoEnumerate = "move-filters-into-enumerate";

function optimizerRuleMdi2dIndexTestSuite() {
  const colName = "UnitTestMdiIndexCollection";
  let col;

  return {
    setUpAll: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: "mdi",
        name: "mdiIndex",
        fields: ["x", "y"],
        fieldValueTypes: "double",
      });
      // Insert 1001 points
      // (-500, -499.5), (-499.1, -499.4), ..., (0, 0.5), ..., (499.9, 500.4), (500, 500.5)
      db._query(aql`
        FOR i IN 0..1000
          LET x = (i - 500) / 10
          LET y = x + 0.5
          INSERT {x, y, i} INTO ${col}
      `);
    },

    tearDownAll: function () {
      col.drop();
    },

    test1: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER d.i == 0
          RETURN d
      `;
      const res = db
        ._createStatement({ query: query.query, bindVars: query.bindVars })
        .explain();
      const nodeTypes = res.plan.nodes
        .map((n) => n.type)
        .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
      const appliedRules = res.plan.rules;
      assertEqual(
        ["SingletonNode", "EnumerateCollectionNode", "ReturnNode"],
        nodeTypes,
      );
      assertFalse(appliedRules.includes(useIndexes));
      assertFalse(appliedRules.includes(removeFilterCoveredByIndex));
    },

    test1_2: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER d.x >= 0 && d.i == 0
          RETURN d
      `;
      const res = db
        ._createStatement({ query: query.query, bindVars: query.bindVars })
        .explain();
      const nodeTypes = res.plan.nodes
        .map((n) => n.type)
        .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
      const appliedRules = res.plan.rules;
      assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
      assertTrue(appliedRules.includes(useIndexes));
      assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
      assertTrue(appliedRules.includes(moveFiltersIntoEnumerate));
    },

    test2: function () {
      const queries = [
        aql`FOR d IN ${col} FILTER 0 <= d.x && d.x <= 1 RETURN d.x`,
        aql`FOR d IN ${col} FILTER IN_RANGE(d.x, 0, 1, true, true) RETURN d.x`,
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i];
        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
        assertTrue(appliedRules.includes(useIndexes));
        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1], res);
      }
    },

    test3: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER 0 <= d.x && d.y <= 1
          RETURN d.x
      `;
      const explainRes = db
        ._createStatement({ query: query.query, bindVars: query.bindVars })
        .explain();
      const appliedRules = explainRes.plan.rules;
      const nodeTypes = explainRes.plan.nodes
        .map((n) => n.type)
        .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
      assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
      assertTrue(appliedRules.includes(useIndexes));
      assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
      const executeRes = db._query(query.query, query.bindVars);
      const res = executeRes.toArray();
      res.sort();
      assertEqual([0, 0.1, 0.2, 0.3, 0.4, 0.5], res);
    },

    test4: function () {
      const queries = [
        aql`FOR d IN ${col} FILTER 0 >= d.x FILTER 10 <= d.y && d.y <= 16 RETURN d.x`,
        aql`FOR d IN ${col} FILTER 0 >= d.x FILTER IN_RANGE(d.y, 10, 16, true, true) RETURN d.x`,
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i];
        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
        assertTrue(appliedRules.includes(useIndexes));
        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([], res);
      }
    },

    test5: function () {
      const queries = [
        aql`FOR d IN ${col} FILTER 0 >= d.x || d.x >= 10 FILTER d.y >= 0 && d.y <= 11 RETURN d.x`,
        aql`FOR d IN ${col} FILTER 0 >= d.x || d.x >= 10 FILTER IN_RANGE(d.y, 0, 11, true, true) RETURN d.x`,
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i];
        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(
          [
            "SingletonNode",
            "IndexNode",
            "CalculationNode",
            "FilterNode",
            "ReturnNode",
          ],
          nodeTypes,
        );
        assertTrue(appliedRules.includes(useIndexes));
        //assertTrue(appliedRules.includes(removeFilterCoveredByIndex)); -- TODO
        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual(
          [
            -0.1, -0.2, -0.3, -0.4, -0.5, 0, 10, 10.1, 10.2, 10.3, 10.4, 10.5,
          ].sort(),
          res,
        );
      }
    },

    test6: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER 0 == d.x
          RETURN d.x
      `;
      const explainRes = db
        ._createStatement({ query: query.query, bindVars: query.bindVars })
        .explain();
      const appliedRules = explainRes.plan.rules;
      const nodeTypes = explainRes.plan.nodes
        .map((n) => n.type)
        .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
      assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
      assertTrue(appliedRules.includes(useIndexes));
      assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
      const executeRes = db._query(query.query, query.bindVars);
      const res = executeRes.toArray();
      res.sort();
      assertEqual([0].sort(), res);
    },

    test7: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER 0 == d.x && 0 == d.y
          RETURN d.x
      `;
      const explainRes = db
        ._createStatement({ query: query.query, bindVars: query.bindVars })
        .explain();
      const appliedRules = explainRes.plan.rules;
      const nodeTypes = explainRes.plan.nodes
        .map((n) => n.type)
        .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
      assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
      assertTrue(appliedRules.includes(useIndexes));
      assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
      const executeRes = db._query(query.query, query.bindVars);
      const res = executeRes.toArray();
      res.sort();
      assertEqual([].sort(), res);
    },

    test8: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER 0 < d.x && d.y <= 1
          RETURN d.x
      `;
      const explainRes = db
        ._createStatement({ query: query.query, bindVars: query.bindVars })
        .explain();
      const appliedRules = explainRes.plan.rules;
      const nodeTypes = explainRes.plan.nodes
        .map((n) => n.type)
        .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
      assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
      assertTrue(appliedRules.includes(useIndexes));
      assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
      const executeRes = db._query(query.query, query.bindVars);
      const res = executeRes.toArray();
      res.sort();
      assertEqual([0.1, 0.2, 0.3, 0.4, 0.5].sort(), res);
    },

    testEstimates: function () {
      let col = db._create(colName + "2");
      col.ensureIndex({
        type: "mdi",
        name: "mdiIndex",
        fields: ["x", "y"],
        fieldValueTypes: "double",
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
          FOR i IN 1..2
            INSERT {x: i, y: i, z: i, stringValue: str} INTO ${col}
      `);
      const index = col.index("mdiIndex");
      assertFalse(index.estimates);
      assertFalse(index.hasOwnProperty("selectivityEstimate"));
      col.drop();
    },

    testTruncate: function () {
      let col = db._create(colName + "3");
      col.ensureIndex({
        type: "mdi",
        name: "mdiIndex",
        fields: ["x", "y"],
        fieldValueTypes: "double",
      });

      db._query(aql`
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i} INTO ${col}
      `);

      let res = db
        ._query(
          aql`
        FOR doc in ${col}
          FILTER doc.x > -100
          RETURN doc
      `,
        )
        .toArray();
      assertEqual(100, res.length);

      col.truncate();

      res = db
        ._query(
          aql`
        FOR doc in ${col}
          FILTER doc.x > -100
          RETURN doc
      `,
        )
        .toArray();
      assertEqual(0, res.length);
      col.drop();
    },

    testFieldValuesTypes: function () {
      let col = db._create(colName + "4");
      const idx = col.ensureIndex({
        type: "mdi",
        name: "mdiIndex",
        fields: ["x", "y"],
        fieldValueTypes: "double",
      });

      assertEqual("double", idx.fieldValueTypes);
      const idx2 = col.index(idx.name);
      assertEqual("double", idx2.fieldValueTypes);
      col.drop();
    },

    testCompareIndex: function () {
      let col = db._create(colName + "4");
      const idx1 = col.ensureIndex({
        type: "mdi",
        fields: ["x", "y"],
        fieldValueTypes: "double",
      });

      const idx2 = col.ensureIndex({
        type: "mdi",
        fields: ["x", "y"],
        fieldValueTypes: "double",
      });

      const idx3 = col.ensureIndex({
        type: "mdi",
        fields: ["y", "x"],
        fieldValueTypes: "double",
      });

      assertEqual(idx1.id, idx2.id);
      assertNotEqual(idx3.id, idx2.id);
      col.drop();
    },

    testCreateAsZkd: function () {
      let col = db._create(colName + "5");
      const idx = col.ensureIndex({
        type: "zkd",
        name: "myIndex",
        fields: ["x", "y"],
        fieldValueTypes: "double",
      });
      assertEqual("zkd", idx.type);

      const idx2 = col.index("myIndex");
      assertEqual(idx2.id, idx.id);
      assertEqual(idx2.type, "zkd");

      col.drop();
    },

    testInRangeStrict: function () {
      const queries = [
        aql`FOR d IN ${col} FILTER 0 < d.x && d.x < 1 RETURN d.x`,
        aql`FOR d IN ${col} FILTER IN_RANGE(d.x, 0, 1, false, false) RETURN d.x`,
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i];
        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
        assertTrue(appliedRules.includes(useIndexes));
        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));

        const indexNodes = explainRes.plan.nodes.filter(function (n) {
          return n.type === "IndexNode";
        });
        assertEqual(indexNodes.length, 1);

        const indexNode = indexNodes[0];
        assertEqual(indexNode.filter, undefined);

        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9], res);
      }
    },

    testInRangeRightStrict: function () {
      const queries = [
        {
          q: aql`FOR d IN ${col} FILTER 0 <= d.x && d.x < 1 RETURN d.x`,
          inRange: false,
        },
        {
          q: aql`FOR d IN ${col} FILTER IN_RANGE(d.x, 0, 1, true, false) RETURN d.x`,
          inRange: true,
        },
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i].q;
        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        assertTrue(appliedRules.includes(useIndexes));

        const indexNodes = explainRes.plan.nodes.filter(function (n) {
          return n.type === "IndexNode";
        });
        assertEqual(indexNodes.length, 1);

        const indexNode = indexNodes[0];
        assertEqual(indexNode.filter, undefined);

        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9], res);
      }
    },

    testInRangeLeftStrict: function () {
      const queries = [
        {
          q: aql`FOR d IN ${col} FILTER 0 < d.x && d.x <= 1 RETURN d.x`,
          inRange: false,
        },
        {
          q: aql`FOR d IN ${col} FILTER IN_RANGE(d.x, 0, 1, false, true) RETURN d.x`,
          inRange: true,
        },
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i].q;
        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);

        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        assertTrue(appliedRules.includes(useIndexes));

        const indexNodes = explainRes.plan.nodes.filter(function (n) {
          return n.type === "IndexNode";
        });
        assertEqual(indexNodes.length, 1);

        const indexNode = indexNodes[0];
        assertEqual(indexNode.filter, undefined);

        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1], res);
      }
    },

    testInRange: function () {
      const queries = [
        aql`FOR d IN ${col} FILTER 0 <= d.x && d.x <= 1 RETURN d.x`,
        aql`FOR d IN ${col} FILTER IN_RANGE(d.x, 0, 1, true, true) RETURN d.x`,
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i];
        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        assertTrue(appliedRules.includes(useIndexes));
        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();

        const indexNodes = explainRes.plan.nodes.filter(function (n) {
          return n.type === "IndexNode";
        });
        assertEqual(indexNodes.length, 1);

        const indexNode = indexNodes[0];
        assertEqual(indexNode.filter, undefined);

        res.sort();
        assertEqual([0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1], res);
      }
    },

    testInRangeMixed: function () {
      const queries = [
        aql`FOR d IN ${col} FILTER IN_RANGE(d.x, 0, 1, false, true) AND 0 <= d.y AND d.y <= 5 RETURN d.x`,
        aql`FOR d IN ${col} FILTER 0 <= d.y AND d.y <= 5 AND IN_RANGE(d.x, 0, 1, false, true) RETURN d.x`,
        aql`FOR d IN ${col} FILTER IN_RANGE(d.y, 0, 5, true, true) AND 0 < d.x AND d.x <= 1 RETURN d.x`,
        aql`FOR d IN ${col} FILTER 0 < d.x AND d.x <= 1 AND IN_RANGE(d.y, 0, 5, true, true) RETURN d.x`,
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i];

        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);

        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        assertTrue(appliedRules.includes(useIndexes));

        const indexNode = explainRes.plan.nodes.find((n) => n.type === "IndexNode");
        assertEqual(indexNode.filter, undefined);

        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1], res);
      }
    },

    testInRangeMultipleInRangeFunctions: function () {
      const queries = [
        aql`FOR d IN ${col} FILTER IN_RANGE(d.x, 0, 1, false, true) AND IN_RANGE(d.y, 0, 5, true, true) RETURN d.x`,
        aql`FOR d IN ${col} FILTER 0 < d.x AND d.x <= 1 AND 0 <= d.y AND d.y <= 5 RETURN d.x`,
      ];

      for (let i = 0; i < queries.length; ++i) {
        const query = queries[i];

        const explainRes = db
          ._createStatement({ query: query.query, bindVars: query.bindVars })
          .explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes
          .map((n) => n.type)
          .filter((n) => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);

        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        assertTrue(appliedRules.includes(useIndexes));

        const indexNode = explainRes.plan.nodes.find((n) => n.type === "IndexNode");
        assertEqual(indexNode.filter, undefined);

        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1], res);
      }
    },
  };
}

// Both MDI indexes share a column family. "second" is created after "first",
// so it has the higher object ID and its entries sort directly after the end
// of "first". Each test writes in a transaction before it queries, so the
// query reads through the transaction's WriteBatchWithIndex iterator.
function mdiIndexInWriteTransactionTestSuite() {
  const colName = "UnitTestMdiIndexTrxCollection";
  let col;

  return {
    setUp: function () {
      col = db._create(colName);
      col.insert({ _key: "z", f1: 1, f3: 2 });
      col.ensureIndex({ type: "mdi", name: "first", fields: ["f1", "f3"],
                        fieldValueTypes: "double", sparse: true });
      col.ensureIndex({ type: "mdi", name: "second", fields: ["f3", "f1"],
                        fieldValueTypes: "double", sparse: true });
    },

    tearDown: function () {
      db._drop(colName);
    },

    // The query uses "first" with the box f3 >= 1. The entry of "second" for
    // "z", read with the fields of "first", is (2, 1) and lies inside that
    // box, so the query must stop at the end of "first" to return "z" once.
    testEntriesOfOtherMdiIndexAreNotReturned: function () {
      const query = `FOR n IN ${colName} FILTER n.f3 >= 1 RETURN n._key`;
      const trx = db._createTransaction({ collections: { write: [colName] } });
      try {
        trx.collection(colName).update("z", { f1: 1 });
        assertEqual(["z"], trx.query(query).toArray());
      } finally {
        trx.abort();
      }
      assertEqual(["z"], db._query(query).toArray());
    },

    // The query uses "first" with the box f3 >= 1.5, which only "z" matches.
    // The entries of "second", read with the fields of "first", are (0, 0),
    // (1, 1) and (2, 1), all outside the box. After them the iterator
    // computes the next z-value and seeks, and that seek must also stop at
    // the end of "first". The update puts the entries of all three documents
    // into the transaction; its value does not matter.
    testQueryEndsAtIndexBoundary: function () {
      col.insert([{ _key: "x", f1: 0, f3: 0 }, { _key: "y", f1: 1, f3: 1 }]);
      const query = `FOR n IN ${colName} FILTER n.f3 >= 1.5 RETURN n._key`;
      const trx = db._createTransaction({ collections: { write: [colName] } });
      try {
        trx.query(`FOR n IN ${colName} UPDATE n WITH { u: 1 } IN ${colName}`);
        assertEqual(["z"], trx.query(query).toArray());
      } finally {
        trx.abort();
      }
    },
  };
}

jsunity.run(optimizerRuleMdi2dIndexTestSuite);
jsunity.run(mdiIndexInWriteTransactionTestSuite);

return jsunity.done();
