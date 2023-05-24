/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertNull, assertNotNull, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2023 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Julia Puget
/// @author Copyright 2023, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const fs = require('fs');
const arangodb = require('@arangodb');
const explainer = require("@arangodb/aql/explainer");
const db = arangodb.db;
const isEnterprise = require("internal").isEnterprise();
let smartGraph = null;
if (isEnterprise) {
  smartGraph = require("@arangodb/smart-graph");
}
const graphModule = require("@arangodb/general-graph");

const vn1 = "testVertex1";
const vn2 = "testVertex2";
const vn3 = "testVertex3";
const cn1 = "edgeTestCollection";
const cn2 = "edgeTestCollection2";
const relationName = "isRelated";
const graphName = "myGraph";
const dbName = "testDatabase";
const fileName = fs.getTempFile() + "-debugDump";
const outFileName = fs.getTempFile() + "-inspectDump";


function recreateEmptyDatabase () {
  db._useDatabase("_system");
  db._dropDatabase(dbName);
  db._createDatabase(dbName);
  db._useDatabase(dbName);
  try {
    db._graphs.document(graphName);
    fail();
  } catch (err) {
    assertEqual(err.errorMessage, "document not found");
  }
}

function createOrderedKeysArray (collName) {
  let keys = db[collName].toArray().map(doc => doc._key);
  keys.sort();
  return keys;
}

function testGeneralWithSmartGraphNamed (amountExamples) {
  const graphBefore = db._graphs.document(graphName);
  delete graphBefore._rev;
  const query = `FOR v,e, p IN 0..10 ANY 'customer/customer-abc' GRAPH '${graphName}'  RETURN {key: e._key}`;
  explainer.debugDump(fileName, query, {}, {examples: amountExamples});
  explainer.inspectDump(fileName, outFileName);
  recreateEmptyDatabase();
  internal.load(outFileName);
  assertNull(db[vn1].any());
  assertNull(db[vn2].any());
  assertNull(db[relationName].any());
  const graphAfter = db._graphs.document(graphName);
  delete graphAfter._rev;
  assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
}

function testGeneralWithGraphNamed (amountExamples) {
  const graphBefore = db._graphs.document(graphName);
  delete graphBefore._rev;
  const query = `FOR v,e, p IN 0..10 ANY  '${vn1}/value-1' GRAPH '${graphName}'  RETURN {vertex: p}`;
  explainer.debugDump(fileName, query, {}, {examples: amountExamples});
  explainer.inspectDump(fileName, outFileName);
  recreateEmptyDatabase();
  internal.load(outFileName);
  const graphAfter = db._graphs.document(graphName);
  delete graphAfter._rev;
  assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
}

function testGeneralWithGraphNoNameInQuery (amountExamples) {
  const collNameBefore = db[cn1].name();
  const query = `WITH ${vn1} FOR v,e, p IN 0..10 ANY  '${vn1}/value-1' ${cn1} RETURN {from: e._from, to: e._to}`;
  explainer.debugDump(fileName, query, {}, {examples: amountExamples});
  explainer.inspectDump(fileName, outFileName);
  recreateEmptyDatabase();
  internal.load(outFileName);
  const collNameAfter = db[cn1].name();
  assertEqual(collNameBefore, collNameAfter);
}

function debugDumpSmartGraphTestsuite() {
  return {
    setUp: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      assertNotNull(smartGraph);
      const rel = smartGraph._relation(relationName, [vn1], [vn2]);
      const namedGraph = smartGraph._create(graphName, [rel], [], {
        smartGraphAttribute: "value",
        isDisjoint: true,
        numberOfShards: 9
      });
      for (let i = 0; i < 10; ++i) {
        db[vn1].insert({_key: "value" + i + ":abc" + i, value: "value" + i});
        db[vn2].insert({_key: "value" + i + ":def" + i, value: "value" + i});
      }
      for (let i = 0; i < 10; ++i) {
        db[relationName].insert({
          _from: vn1 + "/value" + i + ":abc" + i,
          _to: vn2 + "/value" + i + ":def" + i,
          _key: "value" + i + ":" + i + "123:" + "value" + i,
          value: "value" + i
        });
      }
      const newGraph = smartGraph._graph(graphName);
      assertTrue(newGraph.hasOwnProperty(vn1));
      assertTrue(newGraph.hasOwnProperty(vn2));
      assertTrue(newGraph.hasOwnProperty(relationName));
      namedGraph._addVertexCollection(vn3);
      db._createEdgeCollection(cn1);
      db[cn1].save(`${vn1}/pet-dog`, `${vn2}/customer-abc`, {type: "belongs-to"});
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
      try {
        fs.unlink(fileName);
      } catch (err) {
      }
      try {
        fs.unlink(outFileName);
      } catch (err) {
      }
    },

    testDebugDumpWithSmartGraphNamed: function () {
      testGeneralWithGraphNamed(0);
    },

    testDebugDumpWithSmartGraphNamedWithExamples: function () {
      let vn1Before = createOrderedKeysArray(vn1);
      let vn2Before = createOrderedKeysArray(vn2);
      let relationBefore = createOrderedKeysArray(relationName);
      testGeneralWithGraphNamed(10);
      let vn1After = createOrderedKeysArray(vn1);
      let vn2After = createOrderedKeysArray(vn2);
      let relationAfter = createOrderedKeysArray(relationName);
      assertEqual(vn1Before, vn1After);
      assertEqual(vn2Before, vn2After);
      assertEqual(relationBefore, relationAfter);
    },
  };
}



function debugDumpGraphTestsuite() {
  return {
    setUp: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create(vn1);
      db._create(vn2);
      db._createEdgeCollection(cn1);
      db._createEdgeCollection(cn2);
      for (let i = 0; i < 10; ++i) {
        db[vn1].save({_key: `value-${i}`, value: i, type: 'value'});
        db[vn2].save({_key: `test-${i}`, test: i, type: 'test'});
      }
      for (let i = 0; i < 10; ++i) {
        db[cn1].save(`${vn1}/value-${i}`, `${vn2}/test-${i}`, {type: "is-in"});
        if (i < 9) {
          db[cn2].save(`${vn1}/value-${i}`, `${vn1}/value-${i + 1}`, {type: "is-in"});
        }
      }
      const rel1 = graphModule._relation(cn1, vn1, vn2);
      const rel2 = graphModule._relation(cn2, vn1, vn1);
      graphModule._create(graphName, [rel1, rel2], null);
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
      try {
        fs.unlink(fileName);
      } catch (err) {
      }
      try {
        fs.unlink(outFileName);
      } catch (err) {
      }
    },

    testDebugDumpWithGraphNamed: function () {
      testGeneralWithGraphNamed(0);
      assertNull(db[vn1].any());
      assertNull(db[vn2].any());
      assertNull(db[cn1].any());
      assertNull(db[cn2].any());
    },

    testDebugDumpWithGraphNamedWithExamples: function () {
      let vn1Before = createOrderedKeysArray(vn1);
      let vn2Before = createOrderedKeysArray(vn2);
      let cn1Before = createOrderedKeysArray(cn1);
      let cn2Before = createOrderedKeysArray(cn2);
      testGeneralWithGraphNamed(10);
      let vn1After = createOrderedKeysArray(vn1);
      let vn2After = createOrderedKeysArray(vn2);
      let cn1After = createOrderedKeysArray(cn1);
      let cn2After = createOrderedKeysArray(cn2);
      assertEqual(vn1Before, vn1After);
      assertEqual(vn2Before, vn2After);
      assertEqual(cn1Before, cn1After);
      assertEqual(cn2Before, cn2After);
    },

    testDebugDumpWithGraphNoNameInQuery: function () {
      testGeneralWithGraphNoNameInQuery(0);
      assertNull(db[cn1].any());
    },

    testDebugDumpWithGraphNoNameInQueryWithExamples: function () {
      let cn1Before = createOrderedKeysArray(cn1);
      testGeneralWithGraphNoNameInQuery(10);
      let cn1After = createOrderedKeysArray(cn1);
      assertEqual(cn1Before, cn1After);
    },
  };
}


if (isEnterprise) {
  jsunity.run(debugDumpSmartGraphTestsuite);
}
jsunity.run(debugDumpGraphTestsuite);

return jsunity.done();
