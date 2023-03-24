/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertUndefined, assertNull, fail, AQL_EXPLAIN */


// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2023 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const cn = "UnitTestsCollection";
const db = arangodb.db;
const numDocs = 10000;

function InsertMultipleDocumentsSuite() {
    'use strict';

    return {

        setUp: function () {
            db._drop(cn);
            db._create(cn);
        },

        tearDown: function () {
            db._drop(cn);
        },

        testInsertMultipleDocumentsFromVariable: function () {
            let docs = [];
            for (let i = 0; i < numDocs; ++i) {
                docs.push({d: i});
            }
            const res = AQL_EXPLAIN(`FOR d IN @docs INSERT d INTO ${cn}`, {docs: docs});
            const nodes = res.plan.nodes;
            assertEqual(nodes.length, 3);
            assertEqual(nodes[0].type, "SingletonNode");
            assertEqual(nodes[0].dependencies.length, 0);
            let dependencyId = nodes[0].id;
            assertEqual(nodes[1].type, "CalculationNode");
            assertEqual(nodes[1].dependencies.length, 1);
            assertEqual(nodes[1].dependencies[0], dependencyId);
            dependencyId = nodes[1].id;
            assertEqual(nodes[1].outVariable.name, dependencyId);
            assertEqual(nodes[1].expression.type, "array");
            assertEqual(nodes[1].expression.raw.length, numDocs);
            assertEqual(nodes[1].dependencies.length, 1);
            assertEqual(nodes[2].type, "MultipleRemoteOperationNode");
            assertEqual(nodes[2].dependencies.length, 1);
            assertEqual(nodes[2].dependencies[0], dependencyId);
            assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);

            const docsAfterInsertion = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
            docsAfterInsertion.forEach((el, idx) => {
                assertEqual(el, docs[idx]);
            });
        },


    };
}

function InsertMultipleDocumentsShardsSuite() {
    'use strict';

    return {

        setUp: function () {
            db._drop(cn);
            db._create(cn, {numberOfShards: 5, replicationFactor: 1});
        },

        tearDown: function () {
            db._drop(cn);
        },

        testInsertMultipleDocumentsFromVariable: function () {
            let docs = [];
            for (let i = 0; i < numDocs; ++i) {
                docs.push({d: i});
            }
            const res = AQL_EXPLAIN(`FOR d IN @docs INSERT d INTO ${cn}`, {docs: docs});
            const nodes = res.plan.nodes;
            assertEqual(nodes.length, 3);
            assertEqual(nodes[0].type, "SingletonNode");
            assertEqual(nodes[0].dependencies.length, 0);
            let dependencyId = nodes[0].id;
            assertEqual(nodes[1].type, "CalculationNode");
            assertEqual(nodes[1].dependencies.length, 1);
            assertEqual(nodes[1].dependencies[0], dependencyId);
            dependencyId = nodes[1].id;
            assertEqual(nodes[1].outVariable.name, dependencyId);
            assertEqual(nodes[1].expression.type, "array");
            assertEqual(nodes[1].expression.raw.length, numDocs);
            assertEqual(nodes[1].dependencies.length, 1);
            assertEqual(nodes[2].type, "MultipleRemoteOperationNode");
            assertEqual(nodes[2].dependencies.length, 1);
            assertEqual(nodes[2].dependencies[0], dependencyId);
            assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);

            const docsAfterInsertion = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
            docsAfterInsertion.forEach((el, idx) => {
                assertEqual(el, docs[idx]);
            });
        },


    };
}

jsunity.run(InsertMultipleDocumentsSuite);
jsunity.run(InsertMultipleDocumentsShardsSuite);

return jsunity.done();
