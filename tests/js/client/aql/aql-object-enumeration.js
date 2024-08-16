/* global fail */
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
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");

const database = "ObjectEnumTestDB";
const docCollection = "coll";
const edgeColl = "edgeColl";

function enumerateObjectTestSuite() {

  function checkPlan(plan, isOptimized = true) {
    const nodes = plan.nodes;

    // get the EnumerateListNode
    const enumerate = nodes.filter(x => x.type === "EnumerateListNode");
    assertEqual(enumerate.length, 1);


    if (isOptimized) {
      // check that the mode is set to object
      assertEqual(enumerate[0].mode, "object");
      // test that the optimizer rule replace-entries-with-object-iteration triggered
      assertTrue(plan.rules.includes("replace-entries-with-object-iteration"));
    } else {
      assertEqual(enumerate[0].mode, undefined);
    }
  };

  function queryDocuments(coll, query, isOptimized = true) {
    let rules = {optimizer: {rules: ["+replace-entries-with-object-iteration"]}};
    if (!isOptimized) {
      rules = {optimizer: {rules: ["-replace-entries-with-object-iteration"]}};
    }
    const stmt = db._createStatement({query: query, bindVars: null, options: rules});
    checkPlan(stmt.explain().plan, isOptimized);

    return stmt.execute().toArray();
  }

  function queryDocumentsWithLimit(coll, isOptimized = true) {
    const query = `
    FOR doc IN ${coll}
      FOR [key, value] IN ENTRIES(doc)
      LIMIT 12, 40
      RETURN [key, value]
    `;

    return queryDocuments(coll, query, isOptimized);
  }

  function queryDocumentsNoLimit(coll, isOptimized = true) {
    const query = `
    FOR doc IN ${coll}
      FOR [key, value] IN ENTRIES(doc)
      RETURN [key, value]
    `;

    return queryDocuments(coll, query, isOptimized);
  }

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      db._create(docCollection);
      db._createEdgeCollection(edgeColl);

      db[docCollection].insert({"key1": "a"});
      db[docCollection].insert({"key2": "b"});
      db[docCollection].insert({"key3": "c"});
      db[docCollection].insert({"key4": "e"});
      db[docCollection].insert({"key5": "f"});
      db[docCollection].insert({"key6": [1,2,3]});

      db[edgeColl].insert({_from: docCollection + "/key1", _to: docCollection + "/key4", "field": {"a": 1}});
      db[edgeColl].insert({_from: docCollection + "/key3", _to: docCollection + "/key4", "field": {"b": 2}});
      db[edgeColl].insert({_from: docCollection + "/key5", _to: docCollection + "/key6", "field": {"c": 3}});
    },

    tearDownAll: function() {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testSimpleObjectIteration : function() {
      const query = `
        FOR doc IN ${docCollection}
          FOR [key, value] IN ENTRIES(doc)
          RETURN [key, value]
      `;

      const stmt = db._createStatement({query: query, bindVars: null, options: null});
      checkPlan(stmt.explain().plan);
    },

    testOutputDocumentCollection : function() {
      let actual = queryDocumentsNoLimit(docCollection);

      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      assertEqual(systemFields.length, 3 * 6); // 3 system fields * 6 documents in collection
      actual = actual.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [ 
        "key1", 
        "a", 
        "key2", 
        "b", 
        "key3", 
        "c", 
        "key4", 
        "e", 
        "key5", 
        "f", 
        "key6", 
        [ 
          1, 
          2, 
          3
        ] 
      ];
      assertEqual(actual, expected);
    },

    testOutputEdgeCollection : function() {
      let actual = queryDocumentsNoLimit(edgeColl);
      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id", "_from", "_to"].includes(e[0]));
      assertEqual(systemFields.length, 5 * 3); // 5 system fields * 3 documents in collection
      actual = actual.filter((e) => !["_key", "_rev", "_id", "_from", "_to"].includes(e[0])).flat();

      let expected = [ 
        "field", 
        {"a": 1}, 
        "field", 
        {"b": 2}, 
        "field", 
        {"c": 3}
      ];
      assertEqual(actual, expected);
    },

    testOutputDocumentCollectionWithLimit : function() {
      let actual = queryDocumentsWithLimit(docCollection);
      
      // Check that system fields are present
      let systemFields = actual.filter((e) => ["_key", "_rev", "_id"].includes(e[0]));
      assertEqual(systemFields.length, 3 * 3); // 3 system fields * 3 non-skipped documents in collection
      actual = actual.filter((e) => !["_key", "_rev", "_id"].includes(e[0])).flat();
      let expected = [
        "key4", 
        "e", 
        "key5", 
        "f", 
        "key6", 
        [ 
          1, 
          2, 
          3
        ] 
      ];
      assertEqual(actual, expected);
    }

  };
}

jsunity.run(enumerateObjectTestSuite);

return jsunity.done();
