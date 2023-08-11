/*jshint globalstrict:true, strict:true, esnext: true */
/*global AQL_EXPLAIN, assertEqual, assertFalse */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const jsunity = require("jsunity");
const { deriveTestSuite } = require('@arangodb/test-helper');

function VPackIndexInAggregationSuite (unique) {
  const cn = "UnitTestsCollection";
  const n = 10;

  return {
    setUpAll : function () {
      db._drop(cn);
      let c = db._create(cn);

      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value1: String(i).padStart(3, "0") });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value1"], name: "UnitTestsIndex", unique });
    },
    
    tearDownAll : function () {
      db._drop(cn);
    },
    
    testVPackAggregateBySingleAttributeUsingIn: function () {
      for (let i = 1; i < 4; i += 1) {
        const values = [];
        for (let j = 0; j < i; ++j) {
          values.push(String(j).padStart(3, "0"));
        }
        const q = `LET values = @values FOR doc IN ${cn} FILTER doc.value1 IN values COLLECT WITH COUNT INTO count RETURN count`;

        const opts = {};

        let nodes = AQL_EXPLAIN(q, {values}, opts).plan.nodes;
        let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
        assertEqual(1, indexNodes.length);
        assertEqual(1, indexNodes[0].indexes.length);
        assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

        let qr = db._query(q, {values}, opts);
        let results = qr.toArray();
        assertEqual(i, results[0]);
      }
    },
    
    testVPackAggregateBySingleAttributeUsingOr: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value1 == "000" OR doc.value1 == "001" COLLECT WITH COUNT INTO count RETURN count`;

      let nodes = AQL_EXPLAIN(q).plan.nodes;
      let indexNodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, indexNodes.length);
      assertEqual(1, indexNodes[0].indexes.length);
      assertEqual("UnitTestsIndex", indexNodes[0].indexes[0].name);

      let qr = db._query(q);
      let results = qr.toArray();
      assertEqual(2, results[0]);
    },
    
  };
}

function VPackIndexInUniqueSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexInAggregationSuite(/*unique*/ true), suite, '_in_unique');
  return suite;
}

function VPackIndexInNonUniqueSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(VPackIndexInAggregationSuite(/*unique*/ false), suite, '_in_nonUnique');
  return suite;
}

jsunity.run(VPackIndexInUniqueSuite);
jsunity.run(VPackIndexInNonUniqueSuite);

return jsunity.done();
