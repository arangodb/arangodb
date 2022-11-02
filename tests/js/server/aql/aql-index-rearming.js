/*jshint globalstrict:true, strict:true, esnext: true */
/*global AQL_EXPLAIN, assertEqual, assertFalse */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const jsunity = require("jsunity");
const { deriveTestSuite } = require('@arangodb/test-helper');

function PrimaryIndexSuite () {
  const cn = "UnitTestsCollection";
  const n = 10 * 1000;

  return {
    setUpAll : function () {
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ _key: "test" + i, value: i });
        if (docs.length === 1000) {
          c.insert(docs);
          docs = [];
        }
      }
    },

    tearDownAll : function () {
      db._drop(cn);
    },
    
    testPrimaryLookupBySingleRange : function () {
      const q = `FOR i IN 0..2 FOR doc IN ${cn} RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      // will do a full scan
      assertEqual(0, indexNodes.length);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(0, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(3 * n, results.length);

      for (let i = 0; i < 3 * n; ++i) {
        let doc = results[i];
        assertEqual(i % n, doc);
      }
    },
    
    testPrimaryLookupBySingleRangeCovering : function () {
      const q = `FOR i IN 0..2 FOR doc IN ${cn} RETURN doc._key`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      // will use the primary index to do a scan
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("primary", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(3 * n, results.length);

      let data = {};
      for (let i = 0; i < 3 * n; ++i) {
        let key = results[i];
        if (!data.hasOwnProperty(key)) {
          data[key] = 0;
        }
        data[key]++;
      }
      assertEqual(n, Object.keys(data).length);
      Object.keys(data).forEach((key) => {
        assertEqual(3, data[key]);
      });
    },
    
    testPrimaryLookupBySingleRangeCoveringId : function () {
      const q = `FOR i IN 0..2 FOR doc IN ${cn} RETURN doc._id`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      // will use the primary index to do a scan
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("primary", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(3 * n, results.length);

      let data = {};
      for (let i = 0; i < 3 * n; ++i) {
        let key = results[i];
        if (!data.hasOwnProperty(key)) {
          data[key] = 0;
        }
        data[key]++;
      }
      assertEqual(n, Object.keys(data).length);
      Object.keys(data).forEach((key) => {
        assertEqual(3, data[key]);
      });
    },
    
    testPrimaryLookupInByKey : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._key IN [ CONCAT('test', i), CONCAT('test', i + 100000) ] RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("primary", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(1000, results.length);

      for (let i = 0; i < results.length; ++i) {
        assertEqual(i, results[i]);
      }
    },

    testPrimaryLookupByKey : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._key == CONCAT('test', i) RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("primary", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(1000, results.length);

      for (let i = 0; i < results.length; ++i) {
        assertEqual(i, results[i]);
      }
    },
    
    testPrimaryLookupByKeyCovering : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._key == CONCAT('test', i) RETURN doc._id`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("primary", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(1000, results.length);

      for (let i = 0; i < results.length; ++i) {
        assertEqual(cn + "/test" + i, results[i]);
      }
    },
    
    testPrimaryLookupById : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._id == CONCAT('${cn}/test', i) RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("primary", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(1000, results.length);

      for (let i = 0; i < results.length; ++i) {
        assertEqual(i, results[i]);
      }
    },
    
    testPrimaryLookupByIdCovering : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._id == CONCAT('${cn}/test', i) RETURN doc._key`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("primary", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(1000, results.length);

      for (let i = 0; i < results.length; ++i) {
        assertEqual("test" + i, results[i]);
      }
    },
  };
}

function EdgeIndexSuite () {
  const cn = "UnitTestsCollection";
  const n = 10 * 1000;

  return {
    setUpAll : function () {
      let c = db._createEdgeCollection(cn);
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value: i, _from: "v/test" + i, _to: "v/test" + (i % 100) });
        if (docs.length === 1000) {
          c.insert(docs);
          docs = [];
        }
      }
    },

    tearDownAll : function () {
      db._drop(cn);
    },
    
    testEdgeLookupBySingleRange : function () {
      const q = `FOR i IN 0..2 FOR doc IN ${cn} RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      // will do a full scan
      assertEqual(0, indexNodes.length);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(0, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(3 * n, results.length);

      for (let i = 0; i < 3 * n; ++i) {
        let doc = results[i];
        assertEqual(i % n, doc);
      }
    },

    testEdgeLookupByFrom : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._from == CONCAT('v/test', i) RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("edge", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(1000, results.length);

      for (let i = 0; i < results.length; ++i) {
        assertEqual(i, results[i]);
      }
    },
    
    testEdgeLookupByFromIn : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._from IN [ CONCAT('v/test', i), CONCAT('v/test', i + 100000) ] RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("edge", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(1000, results.length);

      for (let i = 0; i < results.length; ++i) {
        assertEqual(i, results[i]);
      }
    },
    
    testEdgeLookupByTo : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._to == CONCAT('v/test', i) RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("edge", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(10000, results.length);

      let data = {};
      for (let i = 0; i < results.length; ++i) {
        assertFalse(data.hasOwnProperty(i));
        data[i] = 1;
      }
      assertEqual(10000, Object.keys(data).length);
    },

    testEdgeLookupByToIn : function () {
      const q = `FOR i IN 0..999 FOR doc IN ${cn} FILTER doc._to IN [ CONCAT('v/test', i), CONCAT('v/test', i + 100000) ] RETURN doc.value`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("edge", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(999, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(10000, results.length);

      let data = {};
      for (let i = 0; i < results.length; ++i) {
        assertFalse(data.hasOwnProperty(i));
        data[i] = 1;
      }
      assertEqual(10000, Object.keys(data).length);
    },
  };
}

function VPackIndexRearmingSuite (unique) {
  const prefix = "this-is-a-long-string-no-chance-for-sso-";
  const cn = "UnitTestsCollection";
  const n = 10 * 1000;
  const pad = `CONCAT('${prefix}', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i))), i)`;
  const pad2 = `CONCAT('${prefix}', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i + 1))), i + 1)`;
  // the following values will intentionally not match anything
  const padBeyond = `CONCAT('${prefix}', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i + 50000))), i + 50000)`;
  const padBeyond2 = `CONCAT('${prefix}', SUBSTRING('00000', 0, 5 - LENGTH(TO_STRING(i + 50000 + 1))), i + 50000 + 1)`;

  return {
    setUpAll : function () {
      db._drop(cn);
      let c = db._create(cn);

      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value1: prefix + String(i).padStart(5, "0"), value2: i });
        if (docs.length === 1000) {
          c.insert(docs);
          docs = [];
        }
      }

      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"], name: "UnitTestsIndex", unique });
    },
    
    tearDownAll : function () {
      db._drop(cn);
    },
    
    testVPackLookupBySingleRange : function () {
      const q = `FOR i IN 0..2 FOR doc IN ${cn} RETURN doc.value1`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(3 * n, results.length);

      for (let i = 0; i < 3 * n; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i % n).padStart(5, "0"), doc);
      }
    },

    testVPackLookupBySingleAttribute : function () {
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER doc.value1 == ${pad} RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);


      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(n - 2, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupByMultipleAttributes : function () {
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER doc.value1 == ${pad} && doc.value2 == i RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(n - 2, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupByRangeOnFirstAttribute : function () {
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER doc.value1 >= ${pad} && doc.value1 < ${pad2} RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);
      
      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(n - 2, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupByRangeOnSecondAttribute : function () {
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER doc.value1 == ${pad} && doc.value2 >= i && doc.value2 < i + 1 RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      assertEqual(1, stats.cursorsCreated);
      assertEqual(n - 2, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupBySingleAttributeUsingIn : function () {
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER doc.value1 IN [${pad}, 'piff'] RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      // no rearming used here, because IN creates a MultiIndexIterator
      assertEqual(n - 1, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupByMultipleAttributesUsingInAndEq : function () {
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER doc.value1 IN [${pad}, 'piff'] FILTER doc.value2 == i RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      // no rearming used here, because IN creates a MultiIndexIterator
      assertEqual(n - 1, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupByMultipleAttributesUsingInAndRange : function () {
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER doc.value1 IN [${pad}, 'piff'] FILTER doc.value2 >= i && doc.value2 < (i + 1) RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      // no rearming used here, because IN creates a MultiIndexIterator
      assertEqual(n - 1, stats.cursorsCreated);
      assertEqual(0, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupBySingleAttributeWithOredConditions1 : function () {
      // OR-ed conditions, using the same index for the same ranges.
      // however, that that the 2 ranges are equivalent is not detected
      // at query compile time, because the range expressions are too
      // complex to be merged.
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER (doc.value1 >= ${pad} && doc.value1 < ${pad2}) || (doc.value1 >= ${pad} && doc.value1 < ${pad2}) RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(2, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[1].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      // 2 indexes here, and thus 2 cursors. the cursors *are* rearmed
      assertEqual(2, stats.cursorsCreated);
      assertEqual((n - 2) * 2, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },
    
    testVPackLookupBySingleAttributeWithOredConditions2 : function () {
      // OR-ed conditions, using the same index for 2 different ranges.
      // note: the second range will never produce any results
      const q = `FOR i IN 0..${n - 2} FOR doc IN ${cn} FILTER (doc.value1 >= ${pad} && doc.value1 < ${pad2}) || (doc.value1 >= ${padBeyond} && doc.value1 < ${padBeyond2}) RETURN doc`;

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };

      let nodes = AQL_EXPLAIN(q, null, opts).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(2, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[1].name);

      let qr = db._query(q, null, opts);
      let stats = qr.getExtra().stats;
      // 2 indexes here, and thus 2 cursors. the cursors *are* rearmed
      assertEqual(2, stats.cursorsCreated);
      assertEqual((n - 2) * 2, stats.cursorsRearmed);
      let results = qr.toArray();
      assertEqual(n - 1, results.length);

      for (let i = 0; i < n - 1; ++i) {
        let doc = results[i];
        assertEqual(prefix + String(i).padStart(5, "0"), doc.value1);
        assertEqual(i, doc.value2);
      }
    },

  };
}

function VPackIndexUniqueSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexRearmingSuite(/*unique*/ true), suite, '_unique');
  return suite;
}

function VPackIndexNonUniqueSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexRearmingSuite(/*unique*/ false), suite, '_nonUnique');
  return suite;
}

jsunity.run(PrimaryIndexSuite);
jsunity.run(EdgeIndexSuite);
jsunity.run(VPackIndexUniqueSuite);
jsunity.run(VPackIndexNonUniqueSuite);

return jsunity.done();
