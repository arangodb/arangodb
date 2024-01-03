////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");
const normalize = require("@arangodb/aql-helper").normalizeProjections;

function optimizerRuleZkd2dIndexTestSuite() {
  const colName = 'UnitTestZkdIndexCollection';
  let col;

  return {

    tearDown: function () {
      db[colName].drop();
    },

    testSimplePrefix: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'zkd',
        name: 'zkdIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ["z"],
        prefixFields: ["stringValue"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str} INTO ${col}
      `);

      const res = db._query(aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo"
          return [doc.z, doc.stringValue]
      `);

      const result = res.toArray();
      assertEqual(result.length, 3);
      assertEqual(_.uniq(result.map(([a, b]) => b)), ["foo"]);
      assertEqual(result.map(([a, b]) => a).sort(), [5, 6, 7]);
    },

    testMultiPrefix: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'zkd',
        name: 'zkdIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ["z"],
        prefixFields: ["stringValue", "value"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
        FOR v IN [1, 19, -2]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str, value: v} INTO ${col}
      `);

      const res = db._query(aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo" && doc.value == -2
          return [doc.z, doc.stringValue, doc.value]
      `);

      const result = res.toArray();
      assertEqual(result.length, 3);
      assertEqual(_.uniq(result.map(([a, b, c]) => b)), ["foo"]);
      assertEqual(_.uniq(result.map(([a, b, c]) => c)), [-2]);
      assertEqual(result.map(([a, b, c]) => a).sort(), [5, 6, 7]);
    },

    testProjections: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'zkd',
        name: 'zkdIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ["z"],
        prefixFields: ["stringValue", "value"],
      });

      db._query(aql`
        FOR str IN ["foo", "bar", "baz"]
        FOR v IN [1, 19, -2]
          FOR i IN 1..100
            INSERT {x: i, y: i, z: i, stringValue: str, value: v} INTO ${col}
      `);

      const query = aql`
        FOR doc IN ${col}
          FILTER doc.x >= 5 && doc.y <= 7 && doc.stringValue == "foo" && doc.value == -2
          return [doc.z, doc.stringValue, doc.value]
      `;

      const res = db._createStatement({query: query.query, bindVars: query.bindVars}).explain();
      const indexNodes = res.plan.nodes.filter(n => n.type === "IndexNode");
      assertEqual(indexNodes.length, 1);
      const index = indexNodes[0];
      assertTrue(index.indexCoversProjections, true);
      assertEqual(normalize(index.projections), normalize(["z", "stringValue", "value"]));

      const result = db._createStatement({query: query.query, bindVars: query.bindVars}).execute().toArray();
      for (const [a, b, c] of result) {
        assertEqual(b, "foo");
        assertEqual(c, -2);
        assertTrue(5 <= a && a <= 7);
      }
    },

  };
}

jsunity.run(optimizerRuleZkd2dIndexTestSuite);

return jsunity.done();
