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
/// @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");
const normalize = require("@arangodb/aql-helper").normalizeProjections;

function mdiIndexStoredValues() {
  const colName = 'UnitTestMdiIndexCollection';
  let col;

  return {
    setUpAll: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'mdi',
        name: 'mdiIndex',
        fields: ['x', 'y'],
        fieldValueTypes: 'double',
        storedValues: ['x', 'y', 'z', 'w.w', '_id']
      });
      db._query(aql`
        FOR i IN 0..1000
          LET x = i / 100
          LET y = i / 100
          INSERT {x, y, z: [x, y], w: {w: i}} INTO ${col}
      `);
    },

    tearDownAll: function () {
      col.drop();
    },

    testSimpleProjections: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER d.x >= 5 && d.y <= 7
          RETURN [d.w.w, d.z, d._id]
      `;

      const res = db._createStatement({query: query.query, bindVars: query.bindVars}).explain();
      const indexNodes = res.plan.nodes.filter(n => n.type === "IndexNode");
      assertEqual(indexNodes.length, 1);
      const index = indexNodes[0];
      assertTrue(index.indexCoversProjections);
      assertEqual(normalize(index.projections), normalize(["z", ["w", "w"], "_id"]));

      const result = db._createStatement({query: query.query, bindVars: query.bindVars}).execute().toArray();
      assertEqual(result.length, 201); // 5.0 - 7.0
      for (const [w, [a, b], id] of result) {
        assertEqual(a, b, id);
        assertTrue(0 <= w && w <= 1000, id);
      }
    },

    testWithPostFilter: function () {
      const query = aql`
        FOR d IN ${col}
          FILTER d.x > 5 && d.y < 7 && d.z[0] > 5
          RETURN [d.w.w, d.z, d._id]
      `;

      const res = db._createStatement({query: query.query, bindVars: query.bindVars}).explain();
      const indexNodes = res.plan.nodes.filter(n => n.type === "IndexNode");
      assertEqual(indexNodes.length, 1);
      const index = indexNodes[0];
      assertTrue(index.indexCoversProjections);
      assertEqual(normalize(index.projections), normalize(["z", ["w", "w"], "_id"]));
      assertTrue(index.indexCoversFilterProjections, true);
      assertEqual(normalize(index.filterProjections), normalize(["z"]));

      const result = db._createStatement({query: query.query, bindVars: query.bindVars}).execute().toArray();
      assertEqual(result.length, 199); // 5.1 - 6.9
      for (const [w, [a, b], id] of result) {
        assertEqual(a, b, id);
        assertTrue(0 <= w && w <= 1000, id);
      }
    },

  };
}

jsunity.run(mdiIndexStoredValues);

return jsunity.done();
