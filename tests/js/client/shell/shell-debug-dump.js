/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertNull, assertNotNull, fail */

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
/// @author Julia Puget
/// @author Copyright 2023, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

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
const { assert } = require('console');
const analyzers = require('@arangodb/analyzers');

const vn1 = "testVertex1";
const vn2 = "testVertex2";
const vn3 = "testVertex3";
const cn  = "collection";
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

function executeFromDump (filename) {
  internal.startCaptureMode();
  try {
    internal.load(filename);
  } finally {
    internal.stopCaptureMode();
  }
}

function testGeneralWithSmartGraphNamed (amountExamples) {
  const graphBefore = db._graphs.document(graphName);
  delete graphBefore._rev;
  const query = `FOR v, e, p IN 0..10 ANY 'customer/customer-abc' GRAPH '${graphName}'  RETURN {key: e._key}`;
  explainer.debugDump(fileName, query, {}, {examples: amountExamples});
  explainer.inspectDump(fileName, outFileName);
  recreateEmptyDatabase();
  executeFromDump(outFileName);
  const graphAfter = db._graphs.document(graphName);
  delete graphAfter._rev;
  assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
}

function testGeneralWithGraphNamed (amountExamples) {
  const graphBefore = db._graphs.document(graphName);
  delete graphBefore._rev;
  const query = `FOR v, e, p IN 0..10 ANY  '${vn1}/value-1' GRAPH '${graphName}'  RETURN {vertex: p}`;
  explainer.debugDump(fileName, query, {}, {examples: amountExamples});
  explainer.inspectDump(fileName, outFileName);
  recreateEmptyDatabase();
  executeFromDump(outFileName);
  const graphAfter = db._graphs.document(graphName);
  delete graphAfter._rev;
  assertEqual(graphBefore, graphAfter, "graph recreated after debugDump is not the same as the original");
}

function testGeneralWithGraphNoNameInQuery (amountExamples) {
  const collNameBefore = db[cn1].name();
  const query = `WITH ${vn1} FOR v, e, p IN 0..10 ANY  '${vn1}/value-1' ${cn1} RETURN {from: e._from, to: e._to}`;
  explainer.debugDump(fileName, query, {}, {examples: amountExamples});
  explainer.inspectDump(fileName, outFileName);
  recreateEmptyDatabase();
  executeFromDump(outFileName);
  const collNameAfter = db[cn1].name();
  assertEqual(collNameBefore, collNameAfter);
}

function debugDumpSmartGraphTestsuite () {
  return {
    setUp: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      assertNotNull(smartGraph);
      const rel = smartGraph._relation(relationName, [vn1], [vn2]);
      const namedGraph = smartGraph._create(graphName, [rel], [], {
        smartGraphAttribute: "value",
        isDisjoint: false,
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
      testGeneralWithSmartGraphNamed(0);
      assertNull(db[vn1].any());
      assertNull(db[vn2].any());
      assertNull(db[relationName].any());
    },

    testDebugDumpWithSmartGraphNamedWithExamples: function () {
      let vn1Before = createOrderedKeysArray(vn1);
      let vn2Before = createOrderedKeysArray(vn2);
      let relationBefore = createOrderedKeysArray(relationName);
      testGeneralWithSmartGraphNamed(10);
      let vn1After = createOrderedKeysArray(vn1);
      let vn2After = createOrderedKeysArray(vn2);
      let relationAfter = createOrderedKeysArray(relationName);
      assertEqual(vn1Before, vn1After);
      assertEqual(vn2Before, vn2After);
      assertEqual(relationBefore, relationAfter);
    },
  };
}

function debugDumpSmartGraphDisjointTestsuite () {
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

    testDebugDumpWithSmartGraphDisjointNamed: function () {
      testGeneralWithSmartGraphNamed(0);
      assertNull(db[vn1].any());
      assertNull(db[vn2].any());
      assertNull(db[relationName].any());
    },

    testDebugDumpWithSmartGraphDisjointNamedWithExamples: function () {
      let vn1Before = createOrderedKeysArray(vn1);
      let vn2Before = createOrderedKeysArray(vn2);
      let relationBefore = createOrderedKeysArray(relationName);
      testGeneralWithSmartGraphNamed(10);
      let vn1After = createOrderedKeysArray(vn1);
      let vn2After = createOrderedKeysArray(vn2);
      let relationAfter = createOrderedKeysArray(relationName);
      assertEqual(vn1Before, vn1After);
      assertEqual(vn2Before, vn2After);
      assertEqual(relationBefore, relationAfter);
    },
  };
}

function debugDumpGraphTestsuite () {
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

function debugDumpViews () {
  return {
    setUp: function () {
      db._createDatabase(dbName, { replicationFactor: 3, writeConcern: 2 });
      db._useDatabase(dbName);

      analyzers.save("my_delimiter", "delimiter", {"delimiter": "@"});
      analyzers.save("delimiter_hyphen", "delimiter", {"delimiter": "-" });
      analyzers.save("text_en_nostem", "text", { locale: "en", case: "lower", accent: false, stemming: false, stopwords: []});
      
      db._create(cn, {numberOfShards: 3});
      db._create(cn1, {numberOfShards: 3});


      for (let i = 0; i < 50; ++i) {
        db[cn].insert({ value1: i + "@" + i, value2: i});
        db[cn1].insert({ value3: i + "@" + i, value4: i});
      }

      let properties = {"links": {}};
      properties["links"][cn] = {
        "analyzers": [
            "my_delimiter",
            "identity",
            "text_en_nostem"
        ],
        "fields": {
            "value1": {},
            "value2": {}
        }
      };
      properties["links"][cn1] = {
        "analyzers": [
          "my_delimiter",
        ],
        "fields": {
            "value3": {},
            "value4": {}
        }
      };
      db._createView("view", "arangosearch", properties);
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

    testDebugDumpWithViews: function () {
      let query = `for d in view search ANALYZER(d.value1 == "31" or d.value1 == "1", "my_delimiter") OPTIONS {waitForSync: true} return d`;
      let res = db._query(query).toArray();
      assertEqual(res.length, 2);
      explainer.debugDump(fileName, query, {}, {examples: 50, anonymize: false});
      explainer.inspectDump(fileName, outFileName);
      recreateEmptyDatabase();
      executeFromDump(outFileName);

      let analyzers_arr = analyzers.toArray();
      assertEqual(analyzers_arr.length, 15); // 13 system analyzers plus "my_delimiter" and "text_en_nostem"

      res = db._query(query).toArray();
      assertEqual(res.length, 2);
    },
    
    testDebugDumpWithViewsCollectionIds: function () {
      let query = `for d in view search d.value1 == 42 return d`;
      explainer.debugDump(fileName, query, {}, {examples: 50, anonymize: false});
//      explainer.inspectDump(fileName, outFileName);
    
      let data = JSON.parse(fs.readFileSync(fileName).toString());
      assertTrue(data.hasOwnProperty("collections"));
      let c = data.collections;
      assertEqual([cn, cn1].sort(), Object.keys(c).sort());
      assertEqual(cn, c[cn].name);
      assertEqual(cn1, c[cn1].name);

      assertTrue(data.hasOwnProperty("explain"));
      let exp = data.explain;
      assertTrue(exp.hasOwnProperty("plan"));
      let plan = exp.plan;
      assertTrue(plan.hasOwnProperty("collections"));

      assertEqual([
        { "name" : "collection", "type" : "read" }, 
        { "name" : "edgeTestCollection", "type" : "read" }, 
        { "name" : "view", "type" : "read" }, 
      ], plan.collections.sort((l, r) => {
        if (l.name < r.name) {
          return -1;
        } else if (l.name > r.name) {
          return 1;
        }
        return 0;
      }));
    },
  };
}


if (isEnterprise) {
  jsunity.run(debugDumpSmartGraphTestsuite);
  jsunity.run(debugDumpSmartGraphDisjointTestsuite);
}
jsunity.run(debugDumpGraphTestsuite);
jsunity.run(debugDumpViews);

return jsunity.done();
