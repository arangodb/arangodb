/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertNotEqual, assertTrue, assertFalse, assertUndefined, assertNull, fail */

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
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require('internal');
const ERRORS = require("@arangodb").errors;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const isEnterprise = internal.isEnterprise();
const cn = "UnitTestsCollection";
const cn2 = "UnitTestsCollection2";
const db = arangodb.db;
const numDocs = 10000;
const ruleName = "optimize-cluster-multiple-document-operations";
const aqlExplain = (query, bindVars, options) => {
  return arango.POST("/_api/explain", {query, bindVars, options});
};

const assertRuleIsUsed = (query, bind = {}, options = {}) => {
  let res = aqlExplain(query, bind, options);
  assertNotEqual(-1, res.plan.rules.indexOf(ruleName));
  const nodes = res.plan.nodes.map((n) => n.type);
  assertNotEqual(-1, nodes.indexOf('MultipleRemoteModificationNode'));
};

const assertRuleIsNotUsed = (query, bind = {}, options = {}) => {
  let res = aqlExplain(query, bind, options);
  assertEqual(-1, res.plan.rules.indexOf(ruleName));
  const nodes = res.plan.nodes.map((n) => n.type);
  assertEqual(-1, nodes.indexOf('MultipleRemoteModificationNode'));
};

function InsertMultipleDocumentsSuite(params) {
  'use strict';
  const {numberOfShards, replicationFactor} = params;
  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn, {numberOfShards, replicationFactor});
    },

    tearDown: function () {
      db._drop(cn);
    },

    testFromEnumerateListInvalidInputsNoArray: function () {
      const queries = [
        `LET docs = MERGE({}, {}) FOR doc IN docs INSERT doc INTO ${cn}`,
        `LET docs = NOOPT({}) FOR doc IN docs INSERT doc INTO ${cn}`,
        `LET docs = {} FOR doc IN docs INSERT doc INTO ${cn}`,
        `FOR doc IN {} INSERT doc INTO ${cn}`,
        `FOR doc IN NOOPT({}) INSERT doc INTO ${cn}`,
      ];
      queries.forEach((query) => {
        try {
          // rule may or may not be applied here right now. 
          // but at least we verify that nothing bad happens if the rule 
          // is applied for these queries in the future.
          db._query(query);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_QUERY_ARRAY_EXPECTED.code, err.errorNum);
        }
      });

      assertEqual(0, db[cn].count());
    },

    testFromEnumerateListInvalidInputsNoObjects: function () {
      const queries = [
        `FOR doc IN [[]] INSERT doc INTO ${cn}`,
        `FOR doc IN [1] INSERT doc INTO ${cn}`,
        `FOR doc IN ["foo"] INSERT doc INTO ${cn}`,
        `FOR doc IN [true] INSERT doc INTO ${cn}`,
      ];
      queries.forEach((query) => {
        try {
          assertRuleIsUsed(query);
          db._query(query);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        }
      });

      assertEqual(0, db[cn].count());
    },

    testFromEnumerateListInvalidInputsNoObjectsIgnoreErrors: function () {
      // this is to be compatible with the execution without the optimization rule
      // "optimize-cluster-multiple-document-operations"
      // when "cluster-one-shard" is not enabled. Independent of the value of "ignoreErrors", the query would fail
      // because of invalid document type

      const queries = [
        `FOR doc IN [[], {_key:"test"}] INSERT doc INTO ${cn} OPTIONS {ignoreErrors: true}`,
        `FOR doc IN [1, {_key:"test"}] INSERT doc INTO ${cn} OPTIONS {ignoreErrors: true}`,
        `FOR doc IN ["foo", {_key:"test"}] INSERT doc INTO ${cn} OPTIONS {ignoreErrors: true}`,
        `FOR doc IN [true, {_key:"test"}] INSERT doc INTO ${cn} OPTIONS {ignoreErrors: true}`,
      ];
      queries.forEach((query) => {
        assertRuleIsUsed(query);
        try {
          db._query(query);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        } finally {
          db[cn].truncate();
        }
      });
    },

    testFromEnumerateListIgnoreErrors: function () {
      for (const ignoreErrors of [false, true]) {
        const queries = [
          `LET list = [{_key: "123"}, {_key: "123"}] FOR d in list INSERT d INTO ${cn} OPTIONS {ignoreErrors: ${ignoreErrors}}`,
          `FOR d in [{_key: "123"}, {_key: "abc"}, {_key: "123"}] LET i = d.value + 3 INSERT d INTO ${cn} OPTIONS {ignoreErrors: ${ignoreErrors}}`,
        ];
        queries.forEach((query) => {
          assertRuleIsUsed(query);
          const previousCount = db[cn].count();
          try {
            db._query(query);
            if (!ignoreErrors) {
              fail();
            } else {
              const afterCount = db[cn].count();
              assertEqual(afterCount, previousCount + 1);
            }
          } catch (err) {
            const afterCount = db[cn].count();
            assertEqual(afterCount, previousCount);
            assertFalse(ignoreErrors);
            assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
            assertTrue(err.errorMessage.includes("unique constraint violated"));
          }
        });
      }
    },

    testTransactionalBehavior: function () {
      let docs = [];
      for (let i = 0; i < 1001; ++i) {
        docs.push({_key: "test" + i});
      }
      docs.push({_key: "test0"});
      const query = `FOR doc IN @docs INSERT doc INTO ${cn}`;
      for (const enableRule of [true, false]) {
        const previousCount = db[cn].count();
        const queryOptions = enableRule ? {} : {optimizer: {rules: ["-optimize-cluster-multiple-document-operations"]}};
        try {
          if (enableRule) {
            assertRuleIsUsed(query, {docs}, queryOptions);
          } else {
            assertRuleIsNotUsed(query, {docs}, queryOptions);
          }
          db._query(query, {docs}, queryOptions);
          fail();
        } catch (err) {
          assertEqual(db[cn].count(), previousCount);
          assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
          assertTrue(err.errorMessage.includes("unique constraint violated"));
        }
        assertEqual(0, db[cn].count());
      }
    },

    testTransactionalBehaviorInvalidInputNotInsert: function () {
      const query = `FOR doc IN [{value1: 1}, true] INSERT doc INTO ${cn}`;
      for (const enableRule of [true, false]) {
        const previousCount = db[cn].count();
        const queryOptions = enableRule ? {} : {optimizer: {rules: ["-optimize-cluster-multiple-document-operations"]}};
        try {
          if (enableRule) {
            assertRuleIsUsed(query, {}, queryOptions);
          } else {
            assertRuleIsNotUsed(query, {}, queryOptions);
          }
          db._query(query, {}, queryOptions);
          fail();
        } catch (err) {
          assertEqual(db[cn].count(), previousCount);
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
          assertTrue(err.errorMessage.includes("invalid document type"));
        }
        assertEqual(0, db[cn].count());
      }
    },

    testTransactionalBehaviorWithIgnoreErrors: function () {
      let docs = [];
      for (let i = 0; i < 1001; ++i) {
        docs.push({_key: "test" + i});
      }
      docs.push({_key: "test0"});

      const query = `FOR doc IN @docs INSERT doc INTO ${cn} OPTIONS { ignoreErrors: true }`;
      for (const enableRule of [true, false]) {
        const queryOptions = enableRule ? {} : {optimizer: {rules: ["-optimize-cluster-multiple-document-operations"]}};
        if (enableRule) {
          assertRuleIsUsed(query, {docs}, queryOptions);
        } else {
          assertRuleIsNotUsed(query, {docs}, queryOptions);
        }
        let res = db._query(query, {docs}, queryOptions);
        assertEqual([], res.toArray());

        assertEqual(1001, db[cn].count());
      }
    },

    testTransactionalBehaviorInvalidInputWithIgnoreErrorsInsertFirst: function () {
      // this is because the query will raise the error even with ignoreErrors: true,
      // because of the coordinator having to know in which server to store the document,
      // while parsing the documents, would raise the "invalid document type" error even though the
      // OPTIONS {ignoreErrors: true} is in the query, independent of the optimization rule
      // so the "optimize-cluster-multiple-document-operations" should behave according to a query execution without
      // it in the execution plan and without "cluster-one-shard"

      const query = `FOR doc IN [{value1: 1}, true] INSERT doc INTO ${cn} OPTIONS {ignoreErrors: true}`;
      let previousCount = db[cn].count();
      const multipleRule = "-optimize-cluster-multiple-document-operations";
      const oneShardRule = "-cluster-one-shard";
      const removeAllRules = {optimizer: {rules: [multipleRule, oneShardRule]}};
      const removeOneShardRule = {optimizer: {rules: [oneShardRule]}};

      let expectSuccess;
      try {
        assertRuleIsNotUsed(query, {}, removeAllRules);
        db._query(query, {}, removeAllRules).toArray();
        expectSuccess = true;
      } catch (err) {
        expectSuccess = false;
        assertEqual(db[cn].count(), previousCount);
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
      }

      previousCount = db[cn].count();
      try {
        assertRuleIsUsed(query, {}, removeOneShardRule);
        db._query(query, {}, removeOneShardRule).toArray();
        if (!expectSuccess) {
          fail();
        }
      } catch (err) {
        assertFalse(expectSuccess);
        assertEqual(db[cn].count(), previousCount);
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
      }
    },

    testStatsWithoutErrors: function () {
      let docs = [];
      for (let i = 0; i < 1005; ++i) {
        docs.push({_key: "test" + i});
      }

      const query = `FOR doc IN @docs INSERT doc INTO ${cn}`;
      for (const enableRule of [true, false]) {
        const queryOptions = enableRule ? {} : {optimizer: {rules: ["-optimize-cluster-multiple-document-operations"]}};
        if (enableRule) {
          assertRuleIsUsed(query, {docs}, queryOptions);
        } else {
          assertRuleIsNotUsed(query, {docs}, queryOptions);
        }
        let res = db._query(query, {docs}, queryOptions);
        assertEqual([], res.toArray());

        assertEqual(1005, db[cn].count());

        let stats = res.getExtra().stats;
        assertEqual(1005, stats.writesExecuted);
        assertEqual(0, stats.writesIgnored);
        db[cn].truncate();
      }
    },

    testStatsWithIgnoreErrors: function () {
      let docs = [];
      for (let i = 0; i < 1001; ++i) {
        docs.push({_key: "test" + i});
      }
      docs.push({_key: "test0"});

      const query = `FOR doc IN @docs INSERT doc INTO ${cn} OPTIONS { ignoreErrors: true }`;
      for (const enableRule of [true, false]) {
        const queryOptions = enableRule ? {} : {optimizer: {rules: ["-optimize-cluster-multiple-document-operations"]}};
        if (enableRule) {
          assertRuleIsUsed(query, {docs}, queryOptions);
        } else {
          assertRuleIsNotUsed(query, {docs}, queryOptions);
        }
        let res = db._query(query, {docs}, queryOptions);
        assertEqual([], res.toArray());

        assertEqual(1001, db[cn].count());

        let stats = res.getExtra().stats;
        assertEqual(1001, stats.writesExecuted);
        assertEqual(1, stats.writesIgnored);
        db[cn].truncate();
      }
    },

    testReturnOld: function () {
      // initial document
      let query = `FOR d IN [{_key: '123', value: 1}] INSERT d INTO ${cn} OPTIONS { overwrite: false }`;
      assertRuleIsUsed(query);

      let res = db._query(query);
      assertEqual(db[cn].count(), 1);
      assertEqual([], res.toArray());

      query = `FOR d IN [{_key: '123', value: 2}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update' } RETURN {old: OLD}`;
      assertRuleIsNotUsed(query);
      res = db._query(query);

      assertEqual(db[cn].count(), 1);
      let docInfo = res.toArray()[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value, 1);

      query = `FOR d IN [{_key: '123', value: 2}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'replace' } RETURN {old: OLD}`;
      assertRuleIsNotUsed(query);
      res = db._query(query);

      assertEqual(db[cn].count(), 1);
      docInfo = res.toArray()[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value, 2);
    },

    testOverwriteReturnNew: function () {
      // initial document
      let query = `FOR d IN [{_key: '123', value: 1}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`;
      assertRuleIsNotUsed(query);

      let res = db._query(query);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.toArray()[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value, 1);

      query = `FOR d IN [{_key: '123', value: 2}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'replace' } RETURN {old: OLD, new: NEW}`;
      assertRuleIsNotUsed(query);
      res = db._query(query);

      assertEqual(db[cn].count(), 1);
      docInfo = res.toArray()[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value, 1);
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value, 2);
    },

    testOverwriteUpdateMerge: function () {
      let query = `FOR d IN [{_key: '123', value1: 1, value2: {name: 'abc'}}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`;
      assertRuleIsNotUsed(query);

      let res = db._query(query);
      assertEqual(db[cn].count(), 1);
      let docInfo = res.toArray()[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value1, 1);
      assertEqual(docInfo.value2.name, 'abc');

      query = `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: true } RETURN {old: OLD, new: NEW}`;
      assertRuleIsNotUsed(query);
      res = db._query(query);

      assertEqual(db[cn].count(), 1);
      docInfo = res.toArray()[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value1, 1);
      assertEqual(docInfo.old.value2.name, 'abc');
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value1, 2);
      assertEqual(docInfo.new.value2.name, 'abc');
      assertEqual(docInfo.new.value2.value3, 'a');
    },

    testOverwriteUpdateNotMerge: function () {
      let query = `FOR d IN [{_key: '123', value1: 1, value2: {name: 'abc'}}] INSERT d INTO ${cn} OPTIONS { overwrite: false } RETURN NEW`;
      assertRuleIsNotUsed(query);
      let res = db._query(query);

      assertEqual(db[cn].count(), 1);
      let docInfo = res.toArray()[0];
      assertEqual(docInfo._key, '123');
      assertEqual(docInfo.value1, 1);
      assertEqual(docInfo.value2.name, 'abc');

      query = `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: false } RETURN {old: OLD, new: NEW}`;
      res = db._query(query);

      assertRuleIsNotUsed(query);
      assertEqual(db[cn].count(), 1);
      docInfo = res.toArray()[0];
      assertEqual(docInfo.old._key, '123');
      assertEqual(docInfo.old.value1, 1);
      assertEqual(docInfo.old.value2.name, 'abc');
      assertEqual(docInfo.new._key, '123');
      assertEqual(docInfo.new.value1, 2);
      assertFalse(docInfo.new.value2.hasOwnProperty('name'));
      assertEqual(docInfo.new.value2.value3, 'a');
    },

    testTrailingReturn: function () {
      let docs = [];
      for (let i = 0; i < numDocs; ++i) {
        docs.push({d: i});
      }
      const expected = {writesExecuted: numDocs, writesIgnored: 0};
      const query = `FOR d IN @docs INSERT d INTO ${cn} OPTIONS { waitForSync: false } RETURN 1`;
      assertRuleIsNotUsed(query, {docs});
      const res = db._query(query, {docs});
      assertEqual(res.toArray().length, numDocs);
      const stats = res.getExtra().stats;
      assertEqual(db[cn].count(), numDocs);
      assertEqual(expected.writesExecuted, stats.writesExecuted);
      assertEqual(expected.writesIgnored, stats.writesIgnored);
    },

    testWaitForSync: function () {
      let docs = [];
      for (let i = 0; i < numDocs; ++i) {
        docs.push({d: i});
      }
      const expected = {writesExecuted: numDocs, writesIgnored: 0};
      const query = `FOR d IN @docs INSERT d INTO ${cn} OPTIONS { waitForSync: false }`;
      assertRuleIsUsed(query, {docs});
      const res = db._query(query, {docs});
      assertEqual([], res.toArray());
      const stats = res.getExtra().stats;
      assertEqual(db[cn].count(), numDocs);
      assertEqual(expected.writesExecuted, stats.writesExecuted);
      assertEqual(expected.writesIgnored, stats.writesIgnored);
    },

    testConflict: function () {
      db[cn].insert({_key: "foobar"});
      assertEqual(1, db[cn].count());
      const query = `FOR d IN [{}, {_key: "foobar"}] INSERT d INTO ${cn}`;
      assertRuleIsUsed(query);
      try {
        db._query(query);
        fail("query did not fail as expected");
      } catch (err) {
        assertTrue(err.errorNum !== undefined, 'unexpected error');
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
        assertEqual(1, db[cn].count());
      }
    },

    testInvalidInput: function () {
      const query = `FOR d IN [{_key: "foobar"}, []] INSERT d INTO ${cn}`;
      assertRuleIsUsed(query);
      try {
        db._query(query);
        fail("query did not fail as expected");
      } catch (err) {
        assertTrue(err.errorNum !== undefined, 'unexpected error');
        assertEqual(internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        assertEqual(0, db[cn].count());
      }
    },

    testExclusive: function () {
      const query = `FOR d IN [{_key: '123', value: "a"}] INSERT d INTO ${cn} OPTIONS {exclusive: true}`;
      assertRuleIsUsed(query);

      const trx = db._createTransaction({collections: {write: cn}});
      trx.collection(cn).insert({_key: '123'});
      let res = arango.POST_RAW('/_api/cursor', JSON.stringify({query: query}), {'x-arango-async': 'store'});
      assertEqual(res.code, 202);
      assertTrue(res.headers.hasOwnProperty("x-arango-async-id"));
      const jobId = res.headers["x-arango-async-id"];
      // for the db server to have time to process the request
      internal.sleep(0.5);
      trx.abort();
      for (let i = 0; i < 10; ++i) {
        res = arango.PUT_RAW('/_api/job/' + jobId, "");
        if (res.code === 201) {
          break;
        } else {
          internal.sleep(0.1);
        }
      }
      assertEqual(res.code, 201, "query not finished in time");
      assertTrue(db[cn].document('123').hasOwnProperty("value"));
      assertEqual(db[cn].document('123').value, "a");
    },

    testFillIndexCache: function () {
      let { instanceRole } = require('@arangodb/testutils/instance');
      let IM = global.instanceManager;
      if (IM.debugCanUseFailAt()) {
        let docs = [];
        for (let i = 0; i < numDocs; ++i) {
          docs.push({d: i});
        }
        try {
          IM.debugSetFailAt("RefillIndexCacheOnFollowers::failIfFalse", instanceRole.dbServer);
          // insert should just work
          db._query(`FOR d IN @docs INSERT d INTO ${cn} OPTIONS {refillIndexCaches: true} RETURN d`, {docs});
          assertEqual(db[cn].count(), numDocs);
        } finally {
          IM.debugClearFailAt('', instanceRole.dbServer);
        }
      }
    },

    testSmartGraph: function () {
      if (!isEnterprise) {
        // SmartGraphs only available in enterprise edition
        return;
      }

      const edges = "SmartEdges";
      const vertex = "SmartVertices";
      const graph = "TestSmartGraph";

      let smartGraphs = require("@arangodb/smart-graph");
      smartGraphs._create(graph, [smartGraphs._relation(edges, vertex, vertex)], null, {
        numberOfShards: numberOfShards,
        smartGraphAttribute: "value"
      });

      try {
        for (let i = 0; i < 10; ++i) {
          db[vertex].insert({_key: "value" + i + ":abc" + i, value: "value" + i});
        }
        for (let i = 0; i < 10; ++i) {
          db[edges].insert({
            _from: vertex + "/value" + i + ":abc" + i,
            _to: vertex + "/value" + (9 - i) + ":abc" + i,
            _key: "value" + i + ":" + i + "123:" + "value" + (9 - i),
            value: "value" + i
          });
        }

        let query = `FOR d IN [{_key: 'value123:abc123', value: 'value123'}] INSERT d INTO ${vertex}`;
        assertRuleIsUsed(query);

        const verticesCount = db[vertex].count();
        let res = db._query(query);
        assertEqual(db[vertex].count(), verticesCount + 1);
        assertEqual([], res.toArray());

        query = `FOR d IN [{_from: '${vertex}/value3:abc3', _to: '${vertex}/value6:abc3', value: 'value3'}] INSERT d INTO ${edges}`;
        assertRuleIsNotUsed(query);

        const edgesCount = db[edges].count();
        res = db._query(query);
        assertEqual(db[edges].count(), edgesCount + 1);
        assertEqual([], res.toArray());
        assertEqual(db["_from_" + edges].count(), db["_to_" + edges].count());

        try {
          query = `FOR d IN [{_from: '${vertex}/value2:abc2', _to: '${vertex}/value3:abc3', value: 'value3'}, {_from: 'value1000:abc1000', _to: '${vertex}/value2:abc2', value: 'value2'}] INSERT d INTO ${edges}`;
          assertRuleIsNotUsed(query);
          db._query(query);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, 1233, "expecting error 1233 to be thrown, got ${err.errorNum}");
          assertEqual(db[edges].count(), edgesCount + 1);
          assertEqual(db["_from_" + edges].count(), db["_to_" + edges].count());
        }
      } finally {
        smartGraphs._drop(graph, true);
      }
    },

  };
}

function InsertMultipleDocumentsExplainSuite(params) {
  'use strict';
  const {numberOfShards, replicationFactor} = params;
  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn, {numberOfShards, replicationFactor});
      db._drop(cn2);
      db._create(cn2, {numberOfShards, replicationFactor});
    },

    tearDown: function () {
      db._drop(cn);
      db._drop(cn2);
    },

    testFromVariable: function () {
      let docs = [];
      for (let i = 0; i < numDocs; ++i) {
        docs.push({d: i});
      }
      const res = aqlExplain(`FOR d IN @docs INSERT d INTO ${cn}`, {docs});
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
      assertEqual(nodes[2].type, "MultipleRemoteModificationNode");
      assertEqual(nodes[2].dependencies.length, 1);
      assertEqual(nodes[2].dependencies[0], dependencyId);
      assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);

      const docsAfterInsertion = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      docsAfterInsertion.forEach((el, idx) => {
        assertEqual(el, docs[idx]);
      });
    },

    testFromEnumerateList: function () {
      const queries = [
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn}`,
        `FOR d in [{value: 1}, {value: 2}] LET i = d.value + 3 INSERT d INTO ${cn}`
      ];
      queries.forEach(query => {
        const res = aqlExplain(query);
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
        assertEqual(nodes[2].type, "MultipleRemoteModificationNode");
        assertEqual(nodes[2].dependencies.length, 1);
        assertEqual(nodes[2].dependencies[0], dependencyId);
        assertEqual(nodes[2].inVariable.name, nodes[1].outVariable.name);
      });
    },

    testEstimateCost: function () {
      const docs = [];
      const numDocs = 100;
      for (let i = 0; i < numDocs; ++i) {
        docs.push({});
      }
      const query = `FOR d in @docs INSERT d INTO ${cn}`;

      const res = aqlExplain(query, {docs: docs});
      const nodes = res.plan.nodes;
      assertEqual(nodes[2].type, "MultipleRemoteModificationNode");
      assertEqual(nodes[2].estimatedNrItems, numDocs);
      assertEqual(nodes[2].estimatedCost, numDocs + 2);
    },

    testSubqueries: function () {
      const queries = [
        `LET docs = (FOR i IN 1..1 FOR d IN ${cn} INSERT d INTO ${cn}) RETURN docs`,
        `LET docs = (FOR i IN 1..1 RETURN i) FOR d IN docs INSERT d INTO ${cn}`,
        `LET illegalList = (FOR x IN ${cn} LIMIT 5 RETURN x)
         LET confusePattern = SLEEP(1)
         FOR d IN illegalList
         INSERT d INTO ${cn}`
      ];
      queries.forEach((query) => {
        assertRuleIsNotUsed(query);
      });
    },

    testInInnerLoop: function () {
      const queries = [
        `FOR i IN 1..1 FOR d IN ${cn} INSERT d INTO ${cn}`,
        `FOR doc IN ${cn} FOR d IN ${cn} INSERT d INTO ${cn}`,
        `FOR i IN 1..10 FOR d IN [{value: 1}, {value: 2}] INSERT d INTO ${cn}`,
        `FOR i IN 1..10 FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn}`,
      ];
      queries.forEach((query) => {
        const rules = {optimizer: {rules: ["-interchange-adjacent-enumerations"]}};
        assertRuleIsNotUsed(query, {}, rules);
      });
    },

    testWithDocumentReadingSubquery: function () {
      const queries = [
        `LET x = (FOR d IN ${cn} LIMIT 5 RETURN d) FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn}`,
      ];
      queries.forEach((query) => {
        assertRuleIsNotUsed(query, {}, {optimizer: {rules: ["-remove-unnecessary-calculations", "-remove-unnecessary-calculations-2", "remove-unnecessary-calculations-3"]}});
      });
    },

    testRuleNoEffect: function () {
      const queries = [
        `FOR d IN 1..1 INSERT d INTO ${cn}`,
        `LET docs = (FOR doc IN ${cn} RETURN doc) FOR d IN docs INSERT d INTO ${cn}`,
        `LET list = NOOPT(APPEND([{value1: 1}], [{value2: "a"}])) FOR d in list INSERT d INTO ${cn}`,
        `FOR d IN [{value: 1}, {value: 2}] LET i = MERGE(d, {}) INSERT i INTO ${cn}`,
        `LET list = [{value: 1}, {value: 2}] FOR d IN list LET merged = MERGE(d, { value2: "abc" }) INSERT merged INTO ${cn}`,
        `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: true } RETURN {old: OLD, new: NEW}`,
        `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} OPTIONS { overwriteMode: 'update', mergeObjects: true } RETURN NEW`,
        `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} RETURN 1`,
        `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} RETURN true`,
        `FOR d in [{value: 1}, {value: 2}] INSERT d INTO ${cn} OPTIONS {exclusive: false} RETURN d`,
        `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] LIMIT 1 INSERT d INTO ${cn}`,
        `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] LIMIT 1, 1 INSERT d INTO ${cn}`,
        `FOR doc IN ${cn} INSERT doc INTO ${cn}`,
        `FOR d IN [{_key: '123', value1: 2, value2: {value3: 'a'}}] INSERT d INTO ${cn} INSERT {value3: 1} INTO ${cn2}`,
        `INSERT [{value1: 1}] INTO ${cn} INSERT [{value2: 1}] INTO ${cn2}`,
        `INSERT [{value1: 1}] INTO ${cn} FOR d IN [{value1: 1}, {value2: 3}] INSERT d INTO ${cn2}`,
      ];
      queries.forEach((query) => {
        assertRuleIsNotUsed(query);
      });
    },

    testRuleEffect: function () {
      const queries = [
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {refillIndexCaches: true}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {refillIndexCaches: false}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {overwrite: true}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {overwrite: false}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {overwriteMode: 'replace'}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {overwriteMode: 'update'}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {overwriteMode: 'ignore'}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {overwriteMode: 'conflict'}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {exclusive: true}`,
        `LET list = [{value: 1}, {value: 2}]  FOR d in list INSERT d INTO ${cn} OPTIONS {exclusive: false}`,
      ];
      const queriesWithVariables = [
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {refillIndexCaches: true}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {refillIndexCaches: false}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {overwrite: true}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {overwrite: false}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {overwriteMode: 'replace'}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {overwriteMode: 'update'}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {overwriteMode: 'ignore'}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {overwriteMode: 'conflict'}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {exclusive: true}`,
        `FOR d in @docs INSERT d INTO ${cn} OPTIONS {exclusive: false}`,
      ];
      queries.forEach(function (query) {
        assertRuleIsUsed(query);
      });
      let docs = [];
      for (let i = 0; i < numDocs; ++i) {
        docs.push({d: i});
      }
      queriesWithVariables.forEach(function (query) {
        assertRuleIsUsed(query, {docs});
      });
    },
  };
}

function InsertMultipleDocumentsSuiteSingleShard() {
  let suite = {};
  deriveTestSuite(
    InsertMultipleDocumentsSuite({numberOfShards: 1, replicationFactor: 1}),
    suite,
    "_SingleShard"
  );
  return suite;
}

function InsertMultipleDocumentsSuiteMultipleShards() {
  let suite = {};
  deriveTestSuite(
    InsertMultipleDocumentsSuite({numberOfShards: 3, replicationFactor: 3}),
    suite,
    "_MultipleShards"
  );
  return suite;
}

function InsertMultipleDocumentsExplainSuiteSingleShard() {
  let suite = {};
  deriveTestSuite(
    InsertMultipleDocumentsExplainSuite({numberOfShards: 1, replicationFactor: 1}),
    suite,
    "_SingleShard"
  );
  return suite;
}

function InsertMultipleDocumentsExplainSuiteMultipleShards() {
  let suite = {};
  deriveTestSuite(
    InsertMultipleDocumentsExplainSuite({numberOfShards: 3, replicationFactor: 3}),
    suite,
    "_MultipleShards"
  );
  return suite;
}

jsunity.run(InsertMultipleDocumentsSuiteSingleShard);
jsunity.run(InsertMultipleDocumentsSuiteMultipleShards);
jsunity.run(InsertMultipleDocumentsExplainSuiteSingleShard);
jsunity.run(InsertMultipleDocumentsExplainSuiteMultipleShards);

return jsunity.done();
