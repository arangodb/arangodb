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

        testInsertMultipleDocumentsFromEnumerateList: function () {
            const queries = [`LET list = [{value: 1}, {vale: 2}]  FOR d in list INSERT d INTO ${cn}`, `FOR d in [{value: 1}, {value: 2}] LET i = d.value + 3 INSERT d INTO ${cn}`];
            queries.forEach(query => {
                const res = AQL_EXPLAIN(query);
                const nodes = res.plan.nodes;
                assertEqual(nodes.length, 3);
                assertEqual(nodes[0].type, "SingletonNode");
                assertEqual(nodes[0].dependencies.length, 0);
                let dependencyId = nodes[0].id;
                assertEqual(nodes[1].type, "CalculationNode");
                assertEqual(nodes[1].dependencies.length, 1);
                assertEqual(nodes[1].dependencies[0], dependencyId);
                dependencyId = nodes[1].id;
                assertEqual(nodes[1].expression.type, "array");
                assertEqual(nodes[1].expression.raw.length, 2);
                assertEqual(nodes[1].dependencies.length, 1);
                assertEqual(nodes[2].type, "MultipleRemoteOperationNode");
                assertEqual(nodes[2].dependencies.length, 1);
                assertEqual(nodes[2].dependencies[0], dependencyId);
                assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);
            });
        },

        testInsertMultipleDocumentsRuleNoEffect: function () {
            const ruleName = "optimize-cluster-multiple-document-operations";
            const queries = [
                `FOR d IN  ${cn} INSERT d INTO ${cn}`,
                `LET list = NOOPT(APPEND([{value1: 1}], [{value2: "a"}])) FOR d in list INSERT d INTO ${cn}`,
                `FOR d IN [{value: 1}, {value: 2}] LET i = MERGE(d, {}) INSERT i INTO ${cn}`,
                `FOR d IN [{value: 1}, {value: 2}] INSERT d INTO ${cn} RETURN d`,
                `FOR d IN ${cn} FOR dd IN d.value INSERT dd INTO ${cn}`,
                `LET list = [{value: 1}, {value: 2}] FOR d IN list LET merged = MERGE(d, { value2: "abc" }) INSERT merged INTO ${cn}`
            ];
            queries.forEach(function (query) {
                const result = AQL_EXPLAIN(query);
                assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
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

        testInsertMultipleDocumentsFromVariableWithShards: function () {
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

        testInsertMultipleDocumentsFromEnumerateListWithShards: function () {
            const queries = [`LET list = [{value: 1}, {vale: 2}]  FOR d in list INSERT d INTO ${cn}`, `FOR d in [{value: 1}, {value: 2}] LET i = d.value + 3 INSERT d INTO ${cn}`,];
            queries.forEach(query => {
                const res = AQL_EXPLAIN(query);
                const nodes = res.plan.nodes;
                assertEqual(nodes.length, 3);
                assertEqual(nodes[0].type, "SingletonNode");
                assertEqual(nodes[0].dependencies.length, 0);
                let dependencyId = nodes[0].id;
                assertEqual(nodes[1].type, "CalculationNode");
                assertEqual(nodes[1].dependencies.length, 1);
                assertEqual(nodes[1].dependencies[0], dependencyId);
                dependencyId = nodes[1].id;
                assertEqual(nodes[1].expression.type, "array");
                assertEqual(nodes[1].expression.raw.length, 2);
                assertEqual(nodes[1].dependencies.length, 1);
                assertEqual(nodes[2].type, "MultipleRemoteOperationNode");
                assertEqual(nodes[2].dependencies.length, 1);
                assertEqual(nodes[2].dependencies[0], dependencyId);
                assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);
            });
        },

        testInsertMultipleDocumentsRuleNoEffectWithShards: function () {
            const ruleName = "optimize-cluster-multiple-document-operations";
            const queries = [
                `FOR d IN  ${cn} INSERT d INTO ${cn}`,
                `LET list = NOOPT(APPEND([{value1: 1}], [{value2: "a"}])) FOR d in list INSERT d INTO ${cn}`,
                `FOR d IN [{value: 1}, {value: 2}] LET i = MERGE(d, {}) INSERT i INTO ${cn}`,
                `FOR d IN [{value: 1}, {value: 2}] INSERT d INTO ${cn} RETURN d`,
                `FOR d IN ${cn} FOR dd IN d.value INSERT dd INTO ${cn}`,
                `LET list = [{value: 1}, {value: 2}] FOR d IN list LET merged = MERGE(d, { value2: "abc" }) INSERT merged INTO ${cn}`
            ];
            queries.forEach(function (query) {
                const result = AQL_EXPLAIN(query);
                assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
            });
        },
    };
}

jsunity.run(InsertMultipleDocumentsSuite);
jsunity.run(InsertMultipleDocumentsShardsSuite);

return jsunity.done();
