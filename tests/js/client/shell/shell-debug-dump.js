/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertNull, assertNotNull, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief recovery tests for views
///
/// @file
///
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


function recreateEmptyDatabase() {
  db._useDatabase("_system");
  try {
    db._dropDatabase(dbName);
  } catch (err) {
  }
  db._createDatabase(dbName);
  db._useDatabase(dbName);
  try {
    db._graphs.document(graphName);
    fail();
  } catch (err) {
    assertEqual(err.errorMessage, "document not found");
  }
}

function debugDumpSmartGraphTestsuite() {
  return {
    setUp: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      assertNotNull(smartGraph);
      try {
        smartGraph._drop(graphName, true);
      } catch (err) {
      }
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
      smartGraph._graph(graphName);
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
      const graphBefore = db._graphs.document(graphName);
      delete graphBefore._rev;
      const query = "FOR v,e, p IN 0..10 ANY 'customer/customer-abc' GRAPH 'myGraph'  RETURN {key: e._key}";
      explainer.debugDump(fileName, query, {}, {});
      explainer.inspectDump(fileName, outFileName);
      recreateEmptyDatabase();
      internal.load(outFileName);
      assertNull(db[vn1].any());
      assertNull(db[vn2].any());
      assertNull(db[relationName].any());
      const graphAfter = db._graphs.document(graphName);
      delete graphAfter._rev;
      assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
    },

    testDebugDumpWithSmartGraphNamedWithExamples: function () {
      let vn1Before = [];
      db[vn1].all().toArray().forEach(vertex => {
        vn1Before.push(vertex._key);
      });
      vn1Before.sort();
      let vn2Before = [];
      db[vn2].all().toArray().forEach(vertex => {
        vn2Before.push(vertex._key);
      });
      vn2Before.sort();
      let relationBefore = [];
      db[relationName].all().toArray().forEach(edge => {
        relationBefore.push(edge._key);
      });
      relationBefore.sort();
      const graphBefore = db._graphs.document(graphName);
      delete graphBefore._rev;
      const query = "FOR v,e, p IN 0..10 ANY 'customer/customer-abc' GRAPH 'myGraph'  RETURN {key: e._key}";
      explainer.debugDump(fileName, query, {}, {examples: 10});
      explainer.inspectDump(fileName, outFileName);
      recreateEmptyDatabase();
      internal.load(outFileName);
      const graphAfter = db._graphs.document(graphName);
      delete graphAfter._rev;
      assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
      let vn1After = [];
      db[vn1].all().toArray().forEach(vertex => {
        vn1After.push(vertex._key);
      });
      vn1After.sort();
      let vn2After = [];
      db[vn2].all().toArray().forEach(vertex => {
        vn2After.push(vertex._key);
      });
      vn2After.sort();
      let relationAfter = [];
      db[relationName].all().toArray().forEach(edge => {
        relationAfter.push(edge._key);
      });
      relationAfter.sort();
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
    },

    testDebugDumpWithGraphNamed: function () {
      const graphBefore = db._graphs.document(graphName);
      delete graphBefore._rev;
      const fileName = fs.getTempFile() + "-debugDump";
      const query = `FOR v,e, p IN 0..10 ANY  '${vn1}/value-1' GRAPH 'myGraph'  RETURN {vertex: p}`;
      explainer.debugDump(fileName, query, {}, {});
      const outFileName = fs.getTempFile() + "-inspectDump";
      explainer.inspectDump(fileName, outFileName);
      recreateEmptyDatabase();
      internal.load(outFileName);
      const graphAfter = db._graphs.document(graphName);
      delete graphAfter._rev;
      assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
      assertNull(db[vn1].any());
      assertNull(db[vn2].any());
      assertNull(db[cn1].any());
      assertNull(db[cn2].any());
    },

    testDebugDumpWithGraphNamedWithExamples: function () {
      let vn1Before = [];
      db[vn1].all().toArray().forEach(vertex => {
        vn1Before.push(vertex._key);
      });
      vn1Before.sort();
      let vn2Before = [];
      db[vn2].all().toArray().forEach(vertex => {
        vn2Before.push(vertex._key);
      });
      vn2Before.sort();
      let cn1Before = [];
      db[cn1].all().toArray().forEach(vertex => {
        cn1Before.push(vertex._key);
      });
      cn1Before.sort();
      let cn2Before = [];
      db[cn2].all().toArray().forEach(vertex => {
        cn2Before.push(vertex._key);
      });
      cn2Before.sort();
      const graphBefore = db._graphs.document(graphName);
      delete graphBefore._rev;
      const fileName = fs.getTempFile() + "-debugDump";
      const query = `FOR v,e, p IN 0..10 ANY  '${vn1}/value-1' GRAPH 'myGraph'  RETURN {vertex: p}`;
      explainer.debugDump(fileName, query, {}, {examples: 10});
      const outFileName = fs.getTempFile() + "-inspectDump";
      explainer.inspectDump(fileName, outFileName);
      recreateEmptyDatabase();
      internal.load(outFileName);
      const graphAfter = db._graphs.document(graphName);
      delete graphAfter._rev;
      assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
      let vn1After = [];
      db[vn1].all().toArray().forEach(vertex => {
        vn1After.push(vertex._key);
      });
      vn1After.sort();
      let vn2After = [];
      db[vn2].all().toArray().forEach(vertex => {
        vn2After.push(vertex._key);
      });
      vn2After.sort();
      let cn1After = [];
      db[cn1].all().toArray().forEach(vertex => {
        cn1After.push(vertex._key);
      });
      cn1After.sort();
      let cn2After = [];
      db[cn2].all().toArray().forEach(vertex => {
        cn2After.push(vertex._key);
      });
      cn2After.sort();
      assertEqual(vn1Before, vn1After);
      assertEqual(vn2Before, vn2After);
      assertEqual(cn1Before, cn1After);
      assertEqual(cn2Before, cn2After);
    },

    testDebugDumpWithGraphNoNameInQuery: function () {
      const collNameBefore = db[cn1].name();
      const fileName = fs.getTempFile() + "-debugDump";
      const query = `FOR v,e, p IN 0..10 ANY  '${vn1}/value-1' ${cn1} RETURN {from: e._from, to: e._to}`;
      explainer.debugDump(fileName, query, {}, {});
      const outFileName = fs.getTempFile() + "-inspectDump";
      explainer.inspectDump(fileName, outFileName);
      recreateEmptyDatabase();
      internal.load(outFileName);
      const collNameAfter = db[cn1].name();
      assertEqual(collNameBefore, collNameAfter);
    },

    testDebugDumpWithGraphNoNameInQueryWithExamples: function () {
      let cn1Before = [];
      db[cn1].all().toArray().forEach(vertex => {
        cn1Before.push(vertex._key);
      });
      cn1Before.sort();
      const collNameBefore = db[cn1].name();
      const fileName = fs.getTempFile() + "-debugDump";
      const query = `FOR v,e, p IN 0..10 ANY  '${vn1}/value-1' ${cn1} RETURN {from: e._from, to: e._to}`;
      explainer.debugDump(fileName, query, {}, {examples: 10});
      const outFileName = fs.getTempFile() + "-inspectDump";
      explainer.inspectDump(fileName, outFileName);
      recreateEmptyDatabase();
      internal.load(outFileName);
      const collNameAfter = db[cn1].name();
      assertEqual(collNameBefore, collNameAfter);
      let cn1After = [];
      db[cn1].all().toArray().forEach(vertex => {
        cn1After.push(vertex._key);
      });
      cn1After.sort();
      assertEqual(cn1Before, cn1After);
    },
  };
}


if (isEnterprise) {
  jsunity.run(debugDumpSmartGraphTestsuite);
}
jsunity.run(debugDumpGraphTestsuite);

return jsunity.done();


