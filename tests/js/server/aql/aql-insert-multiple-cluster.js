/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertUndefined, assertNull, fail, AQL_EXPLAIN, AQL_EXECUTE */


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
const internal = require('internal');
const ERRORS = require("@arangodb").errors;
const helper = require("@arangodb/aql-helper");
const getModifyQueryResults = helper.getModifyQueryResults;
const sanitizeStats = helper.sanitizeStats;
const cn = "UnitTestsCollection";
const db = arangodb.db;
const numDocs = 10000;
const ruleName = "optimize-cluster-multiple-document-operations";

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
      const queries = [`LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn}`, `FOR d in [{value: 1}, {value: 2}] LET i = d.value + 3 INSERT d INTO ${cn}`];
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

    testInsertMultipleDocumentsFromEnumerateListIgnoreErrors: function () {
      let ignoreErrors = false;
      const queries = [`LET list = [{_key: "123"}, {_key: "123"}] FOR d in list INSERT d INTO ${cn} OPTIONS {ignoreErrors: ${ignoreErrors}}`, `FOR d in [{_key: "123"}, {_key: "abc"}, {_key: "123"}] LET i = d.value + 3 INSERT d INTO ${cn} OPTIONS {ignoreErrors: ${ignoreErrors}}`,];
      for (let i = 0; i < 1; ++i) {
        ignoreErrors = (i === 0) ? false : true;
        queries.forEach((query, idx) => {
          let res = AQL_EXPLAIN(query);
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
          assertEqual(nodes[1].expression.raw.length, idx + 2);
          assertEqual(nodes[1].dependencies.length, 1);
          assertEqual(nodes[2].type, "MultipleRemoteOperationNode");
          assertEqual(nodes[2].dependencies.length, 1);
          assertEqual(nodes[2].dependencies[0], dependencyId);
          assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);
          try {
            res = AQL_EXECUTE(query);
            if (!ignoreErrors) {
              fail();
            } else {
              assertTrue(db[cn].count(), idx + 1);
            }
          } catch (err) {
            assertFalse(ignoreErrors);
            assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
            assertTrue(err.errorMessage.includes("unique constraint violated"));
          }
        });
      }
    },

    testInsertMultipleDocumentsOverwriteReplace: function () {
      let res = AQL_EXECUTE(`FOR d IN [{_key: '123', value: 1}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.json[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value, 1);

      res = AQL_EXECUTE(`FOR d IN [{_key: '123', value: 2}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'replace' } RETURN {old: OLD, new: NEW}`);
      assertEqual(db[cn].count(), 1);
      docInfo = res.json[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value, 1);
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value, 2);
    },

    testInsertMultipleDocumentsOverwriteUpsertMerge: function () {
      let res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 1, value2: {name: 'abc'}}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.json[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value1, 1);
      assertEqual(docInfo.value2.name, 'abc');

      res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: true } RETURN {old: OLD, new: NEW}`);
      assertEqual(db[cn].count(), 1);
      docInfo = res.json[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value1, 1);
      assertEqual(docInfo.old.value2.name, 'abc');
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value1, 2);
      assertEqual(docInfo.new.value2.name, 'abc');
      assertEqual(docInfo.new.value2.value3, 'a');
    },

    testInsertMultipleDocumentsOverwriteUpsertNotMerge: function () {
      let res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 1, value2: {name: 'abc'}}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.json[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value1, 1);
      assertEqual(docInfo.value2.name, 'abc');

      res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: false } RETURN {old: OLD, new: NEW}`);
      assertEqual(db[cn].count(), 1);
      docInfo = res.json[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value1, 1);
      assertEqual(docInfo.old.value2.name, 'abc');
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value1, 2);
      assertFalse(docInfo.new.value2.hasOwnProperty('name'));
      assertEqual(docInfo.new.value2.value3, 'a');
    },

    testInsertMultipleDocumentsWaitForSync: function () {
      let docs = [];
      for (let i = 0; i < numDocs; ++i) {
        docs.push({d: i});
      }
      const expected = {writesExecuted: numDocs, writesIgnored: 0};
      const query = `FOR d IN @docs INSERT d INTO ${cn} OPTIONS { waitForSync: true }`;
      const result = AQL_EXPLAIN(query, {docs: docs});
      assertTrue(result.plan.rules.indexOf(ruleName) !== -1, query);
      const actual = getModifyQueryResults(query, {docs: docs});
      assertEqual(db[cn].count(), numDocs);
      assertEqual(expected, sanitizeStats(actual));
    },

    testInsertMultipleDocumentsRuleNoEffect: function () {
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

    testInsertWithConflict: function () {
      db[cn].insert({_key: "foobar"});
      assertEqual(1, db[cn].count());
      try {
        db._query(`FOR d IN [{}, {_key: "foobar"}] INSERT d INTO ${cn}`);
        fail("query did not fail as expected");
      } catch (err) {
        assertTrue(err.errorNum !== undefined, 'unexpected error');
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
        assertEqual(1, db[cn].count());
      }
    }
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
      const queries = [`LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn}`, `FOR d in [{value: 1}, {value: 2}] LET i = d.value + 3 INSERT d INTO ${cn}`,];
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

    testInsertMultipleDocumentsFromEnumerateListIgnoreErrorsWithShards: function () {
      let ignoreErrors = false;
      const queries = [`LET list = [{_key: "123"}, {_key: "123"}] FOR d in list INSERT d INTO ${cn} OPTIONS {ignoreErrors: ${ignoreErrors}}`, `FOR d in [{_key: "123"}, {_key: "abc"}, {_key: "123"}] LET i = d.value + 3 INSERT d INTO ${cn} OPTIONS {ignoreErrors: ${ignoreErrors}}`,];
      for (let i = 0; i < 1; ++i) {
        ignoreErrors = (i === 0) ? false : true;
        queries.forEach((query, idx) => {
          let res = AQL_EXPLAIN(query);
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
          assertEqual(nodes[1].expression.raw.length, idx + 2);
          assertEqual(nodes[1].dependencies.length, 1);
          assertEqual(nodes[2].type, "MultipleRemoteOperationNode");
          assertEqual(nodes[2].dependencies.length, 1);
          assertEqual(nodes[2].dependencies[0], dependencyId);
          assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);
          try {
            res = AQL_EXECUTE(query);
            if (!ignoreErrors) {
              fail();
            } else {
              assertTrue(db[cn].count(), idx + 1);
            }
          } catch (err) {
            assertFalse(ignoreErrors);
            assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
            assertTrue(err.errorMessage.includes("unique constraint violated"));
          }
        });
      }
    },

    testInsertMultipleDocumentsOverwriteReplaceWithShards: function () {
      let res = AQL_EXECUTE(`FOR d IN [{_key: '123', value: 1}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.json[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value, 1);

      res = AQL_EXECUTE(`FOR d IN [{_key: '123', value: 2}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'replace' } RETURN {old: OLD, new: NEW}`);
      assertEqual(db[cn].count(), 1);
      docInfo = res.json[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value, 1);
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value, 2);
    },

    testInsertMultipleDocumentsOverwriteUpsertMergeWithShards: function () {
      let res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 1, value2: {name: 'abc'}}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.json[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value1, 1);
      assertEqual(docInfo.value2.name, 'abc');

      res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: true } RETURN {old: OLD, new: NEW}`);
      assertEqual(db[cn].count(), 1);
      docInfo = res.json[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value1, 1);
      assertEqual(docInfo.old.value2.name, 'abc');
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value1, 2);
      assertEqual(docInfo.new.value2.name, 'abc');
      assertEqual(docInfo.new.value2.value3, 'a');
    },

    testInsertMultipleDocumentsOverwriteUpsertNotMergeWithShards: function () {
      let res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 1, value2: {name: 'abc'}}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.json[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value1, 1);
      assertEqual(docInfo.value2.name, 'abc');

      res = AQL_EXECUTE(`FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: false } RETURN {old: OLD, new: NEW}`);
      assertEqual(db[cn].count(), 1);
      docInfo = res.json[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value1, 1);
      assertEqual(docInfo.old.value2.name, 'abc');
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value1, 2);
      assertFalse(docInfo.new.value2.hasOwnProperty('name'));
      assertEqual(docInfo.new.value2.value3, 'a');
    },

    testInsertMultipleDocumentsWaitForSyncWithShards: function () {
      let docs = [];
      for (let i = 0; i < numDocs; ++i) {
        docs.push({d: i});
      }
      const expected = {writesExecuted: numDocs, writesIgnored: 0};
      const query = `FOR d IN @docs INSERT d INTO ${cn} OPTIONS { waitForSync: true }`;
      const result = AQL_EXPLAIN(query, {docs: docs});
      assertTrue(result.plan.rules.indexOf(ruleName) !== -1, query);
      const actual = getModifyQueryResults(query, {docs: docs});
      assertEqual(db[cn].count(), numDocs);
      assertEqual(expected, sanitizeStats(actual));
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
