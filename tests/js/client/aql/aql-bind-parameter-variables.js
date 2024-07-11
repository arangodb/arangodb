/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const {db} = require("@arangodb");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");

const database = "BindParameterVarsTestDb";
const collection = "MyTestCollection";

function aqlBindParameterVariableTest() {

  const bind = "param";


  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      const C = db._create(collection);
      let docs = [];
      for (let i = 0; i < 100; i++) {
        docs.push({_key: `v${i}`, i, j: 2*i});
      }
      C.save(docs);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testReturnBindParameter: function () {

      const query = `
        RETURN @${bind}
      `;
      const stmt = db._createStatement({query, bindVars: {param: 12}, options: {optimizePlanForCaching: true}});
      const plan = stmt.explain().plan;

      const [singleton, returnNode] = plan.nodes;
      assertEqual(singleton.type, "SingletonNode");
      assertEqual(returnNode.type, "ReturnNode");

      const bindParameter = singleton.bindParameterVariables;
      assertEqual(Object.keys(bindParameter), [bind]);

      assertEqual(bindParameter[bind].id, returnNode.inVariable.id);

      const result = stmt.execute().toArray();
      assertEqual(result, [12]);
    },

    testReturnMultipleBindParameter: function () {

      const query = `
        RETURN [@${bind}, @other, @third]
      `;
      const stmt = db._createStatement({
        query,
        bindVars: {[bind]: 12, other: true, third: "Hello"},
        options: {optimizePlanForCaching: true}
      });
      const plan = stmt.explain().plan;

      const [singleton, calculation, returnNode] = plan.nodes;
      assertEqual(singleton.type, "SingletonNode");
      assertEqual(calculation.type, "CalculationNode");
      assertEqual(returnNode.type, "ReturnNode");

      const bindParameter = singleton.bindParameterVariables;
      assertEqual(Object.keys(bindParameter).sort(), [bind, "other", "third"].sort());


      const result = stmt.execute().toArray();
      assertEqual(result, [[12, true, "Hello"]]);
    },

    testFilterBindParameter: function () {
      const query = `
        FOR i IN 1..20
        FILTER i % @${bind} == 0
        RETURN i
      `;
      const stmt = db._createStatement({query, bindVars: {param: 4}, options: {optimizePlanForCaching: true}});
      const plan = stmt.explain().plan;

      const singleton = plan.nodes[0];
      assertEqual(singleton.type, "SingletonNode");

      const bindParameter = singleton.bindParameterVariables;
      assertEqual(Object.keys(bindParameter), [bind]);

      const result = stmt.execute().toArray();
      assertEqual(result, [4, 8, 12, 16, 20]);
    },

    testFilterDocumentsBindParameter: function () {
      const query = `
        FOR doc IN ${collection}
        FILTER doc.i % @${bind} == 0
        SORT doc.i
        LIMIT 6
        RETURN doc.i
      `;
      const stmt = db._createStatement({query, bindVars: {param: 4}, options: {optimizePlanForCaching: true}});
      const plan = stmt.explain().plan;

      const singleton = plan.nodes[0];
      assertEqual(singleton.type, "SingletonNode");

      const bindParameter = singleton.bindParameterVariables;
      assertEqual(Object.keys(bindParameter), [bind]);

      const result = stmt.execute().toArray();
      assertEqual(result, [0, 4, 8, 12, 16, 20]);
    },

    testFilterDocumentsReturnBindParameter: function () {
      const query = `
        FOR doc IN ${collection}
        SORT doc.i
        LIMIT 10
        RETURN [doc.i, @${bind}]
      `;
      const stmt = db._createStatement({query, bindVars: {param: 4}, options: {optimizePlanForCaching: true}});
      const result = stmt.execute().toArray();
      result.forEach(function ([a, b], idx) {
        assertEqual(a, idx);
        assertEqual(b, 4);
      });
    }

  };
}

jsunity.run(aqlBindParameterVariableTest);

return jsunity.done();
