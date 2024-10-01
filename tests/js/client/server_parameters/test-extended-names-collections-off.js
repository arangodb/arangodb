/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertNull, assertNotNull, fail */

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
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names': "false",
  };
}
const jsunity = require('jsunity');
const internal = require("internal");
const db = internal.db;
const errors = require('@arangodb').errors;
const isCluster = internal.isCluster;
const isEnterprise = internal.isEnterprise;

const traditionalName = "UnitTestsCollection";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";

const runGraphTest = (graph) => {
  const gn = "le-graph";

  const edgeDef = graph._edgeDefinitions(
    graph._relation(traditionalName, [extendedName], [extendedName]),
  );

  try {
    graph._create(gn, edgeDef);
    fail();
  } catch (err) {
    assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
  }
};

function testSuite() {
  return {
    tearDown: function() {
      db._drop(traditionalName);
      db._drop(extendedName);
    },

    testCreateExtendedName: function() {
      try {
        db._create(extendedName);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
      
      let c = db._collection(extendedName);
      assertNull(c);
    },
    
    testCreateEdgeExtendedName: function() {
      try {
        db._createEdgeCollection(extendedName);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
      
      let c = db._collection(extendedName);
      assertNull(c);
    },
    
    testRenameToExtendedName: function() {
      if (isCluster()) {
        // renaming not supported in cluster
        return;
      }
        
      let c = db._create(traditionalName);
      try {
        c.rename(extendedName);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },
    
    testGraphWithExtendedCollectionNames: function() {
      const graph = require("@arangodb/general-graph");
      runGraphTest(graph);
    },
    
    testSmartGraphWithExtendedCollectionNames: function() {
      if (!isEnterprise()) {
        return;
      }

      const graph = require("@arangodb/smart-graph");
      runGraphTest(graph);
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
