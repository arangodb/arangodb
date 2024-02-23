/* global fail */
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
/// @author Tobias Gödderz
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const _ = require("lodash");

const useIndexes = 'use-indexes';
const removeFilterCoveredByIndex = "remove-filter-covered-by-index";

function optimizerRuleMdi2dIndexTestSuite() {
    const colName = 'UnitTestMdiIndexCollection';
    let col;

    return {
        setUpAll: function () {
            col = db._create(colName, {numberOfShards: 2, shardKeys: ["x"]});
            col.ensureIndex({
                type: 'mdi',
                name: 'mdiIndex',
                fields: ['x', 'y'],
                storedValues: ['i'],
                unique: true,
                fieldValueTypes: 'double'
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

        testIndexAccess: function () {
            const query = aql`
        FOR d IN ${col}
          FILTER 0 <= d.x && d.x <= 1
          RETURN d.x
      `;
            const explainRes = db._createStatement({query: query.query, bindVars:  query.bindVars}).explain();
            const appliedRules = explainRes.plan.rules;
            const nodeTypes = explainRes.plan.nodes.map(n => n.type).filter(n => !["GatherNode", "RemoteNode"].includes(n));
            assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
            assertTrue(appliedRules.includes(useIndexes));
            assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
            const executeRes = db._query(query.query, query.bindVars);
            const res = executeRes.toArray();
            res.sort();
            assertEqual([0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1], res);
        },

      testEstimates: function () {
          const index = col.index("mdiIndex");
          assertTrue(index.estimates);
          assertEqual(index.selectivityEstimate, 1);
      },

      testIndexAccess2: function () {
        const query = aql`
        FOR d IN ${col}
          FILTER 0 <= d.x && d.y <= 1
          RETURN d.x
      `;
        const explainRes = db._createStatement({query: query.query, bindVars:  query.bindVars}).explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes.map(n => n.type).filter(n => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
        assertTrue(appliedRules.includes(useIndexes));
        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([0, 0.1, 0.2, 0.3, 0.4, 0.5], res);
      },

      testIndexAccessStoredValues: function () {
        const query = aql`
        FOR d IN ${col}
          FILTER 0 <= d.x && d.y <= 1
          RETURN d.i
      `;
        const explainRes = db._createStatement({query: query.query, bindVars:  query.bindVars}).explain();
        const appliedRules = explainRes.plan.rules;
        const nodeTypes = explainRes.plan.nodes.map(n => n.type).filter(n => !["GatherNode", "RemoteNode"].includes(n));
        assertEqual(["SingletonNode", "IndexNode", "ReturnNode"], nodeTypes);
        assertTrue(appliedRules.includes(useIndexes));
        assertTrue(appliedRules.includes(removeFilterCoveredByIndex));
        const indexNodes = explainRes.plan.nodes.filter(n => n.type === "IndexNode");
        assertEqual(indexNodes.length, 1);
        const index = indexNodes[0];
        assertTrue(index.indexCoversProjections, true);
        assertEqual(normalize(index.projections), normalize(["i"]));
        const executeRes = db._query(query.query, query.bindVars);
        const res = executeRes.toArray();
        res.sort();
        assertEqual([500, 501, 502, 503, 504, 505], res);
      },

        testUniqueConstraint: function () {
            col.save({x: 0, y: 0.50001});
            try {
                col.save({x: 0, y: 0.5});
                fail();
            } catch (e) {
                assertEqual(e.errorNum, internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
            }
        }
    };
}

jsunity.run(optimizerRuleMdi2dIndexTestSuite);

return jsunity.done();
