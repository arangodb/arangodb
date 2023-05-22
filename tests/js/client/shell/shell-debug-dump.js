/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertNotNull, print, fail */

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

const vn1 = "testVertex1";
const vn2 = "testVertex2";
const vn3 = "testVertex3";
const cn1 = "edgeTestCollection";
const relationName = "isRelated";
const smartGraphName = "myGraph";

function debugDumpTestsuite() {
  return {

    setUpAll: function () {
      assertNotNull(smartGraph);
      try {
        smartGraph._drop(smartGraphName, true);
      } catch (err) {
      }
      const rel = smartGraph._relation(relationName, [vn1], [vn2]);
      const namedGraph = smartGraph._create(smartGraphName, [rel], [], {
        smartGraphAttribute: "region",
        isDisjoint: true,
        numberOfShards: 9
      });
      smartGraph._graph(smartGraphName);
      namedGraph._addVertexCollection(vn3);
      db._createEdgeCollection(cn1);
      db[cn1].save(`${vn1}/pet-dog`, `${vn2}/customer-abc`, {type: "belongs-to"});
    },

    tearDownAll: function () {
      try {
        smartGraph._drop(smartGraphName, true);
      } catch (err) {
      }
      db._drop(vn1);
      db._drop(vn2);
      db._drop(vn3);
      db._drop(cn1);
      db._drop(relationName);
    },

    testDebugDumpWithSmartGraphNamed: function () {
      let res = db._query("FOR doc IN _graphs RETURN doc").toArray();
      assertEqual(res.length, 1);
      const graphBefore = res[0];
      const fileName = fs.getTempFile() + "-debugDump";
      const query = "FOR v,e, p IN 0..10 ANY 'customer/customer-abc' GRAPH 'myGraph'  RETURN {key: e._key}";
      explainer.debugDump(fileName, query, {}, {});
      const outFileName = fs.getTempFile() + "-inspectDump";
      explainer.inspectDump(fileName, outFileName);
      try {
        internal.load(outFileName);
        let res = db._query("FOR doc IN _graphs RETURN doc").toArray();
        assertEqual(res.length, 1);
        const graphAfter = res[0];
        for (const key in graphBefore) {
          assertTrue(graphAfter.hasOwnProperty(key));
          if (key !== "_rev") {
            assertEqual(graphBefore[key], graphAfter[key]);
          }
        }
      } catch (err) {
        print("Unable to recreate smart graph after debugDump ", err);
        fail();
      }
    }
  };
}

jsunity.run(debugDumpTestsuite);

return jsunity.done();


