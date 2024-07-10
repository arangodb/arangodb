/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertException */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const {db} = require("@arangodb");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");

const database = "BindParameterVarsTestDb";

function aqlBindParameterVariableTest() {

  const bind = "param";


  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);

      const C = db._create("C");
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
      const stmt = db._createStatement({query, bindVars: {param: 12}, options: {cachePlan: true}});
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
        options: {cachePlan: true}
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
      const stmt = db._createStatement({query, bindVars: {param: 4}, options: {cachePlan: true}});
      const plan = stmt.explain().plan;

      const singleton = plan.nodes[0];
      assertEqual(singleton.type, "SingletonNode");

      const bindParameter = singleton.bindParameterVariables;
      assertEqual(Object.keys(bindParameter), [bind]);

      const result = stmt.execute().toArray();
      assertEqual(result, [4, 8, 12, 16, 20]);
    }

  };
}

jsunity.run(aqlBindParameterVariableTest);

return jsunity.done();
