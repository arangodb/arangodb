/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertFalse, assertNull, fail, AQL_EXECUTE */
////////////////////////////////////////////////////////////////////////////////
/// @brief recovery tests for views
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');
var analyzers = require("@arangodb/analyzers");

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  var c = db._create('UnitTestsRecoveryDummy');

  db._dropView('UnitTestsRecoveryView');
  db._dropView('UnitTestsRecoveryView2');
  try { analyzers.remove('calcAnalyzer', true); } catch(e) {}

  analyzers.save('calcAnalyzer',"aql",{queryString:"RETURN SOUNDEX(@param)"});
  var i1 = c.ensureIndex({ type: "inverted", name: "i1", fields: [ "a", "b", "c" ] });
  var i2 = c.ensureIndex({ type: "inverted", name: "i2", includeAllFields:true, analyzer: "calcAnalyzer", fields: [ "a", "b", "c" ] });
  var i3 = c.ensureIndex({ type: "inverted", name: "i3", includeAllFields:true });

  var meta1 = { indexes: [ { index: i1.name, collection: c.name() } ] };
  var meta2 = { indexes: [ { index: i2.name, collection: c.name() } ] };
  var meta3 = { indexes: [ { index: i3.name, collection: c.name() } ] };
  db._createView('UnitTestsRecoveryView', 'search-alias', meta1);
  db._createView('UnitTestsRecoveryView2', 'search-alias', meta2);
  db._createView('UnitTestsRecoveryView3', 'search-alias', meta3);
  db._createView('UnitTestsRecoveryView4', 'search-alias', {});
  db._createView('UnitTestsRecoveryView5', 'search-alias', {});
  db._view('UnitTestsRecoveryView4').properties(meta1);
  db._view('UnitTestsRecoveryView5').properties(meta2);

  for (let i = 0; i < 50000; i++) {
    c.save({ a: "foo_" + i, b: "bar_" + i, c: i });
  }

  c.save({ name: 'crashme' }, { waitForSync: true });

  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {},
    tearDown: function () {},

    testIResearchLinkPopulate: function () {
      let checkView = function(viewName, indexName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(1, indexes.length);
        assertEqual(indexName, indexes[0].index);
        assertEqual("UnitTestsRecoveryDummy", indexes[0].collection);
      };

      checkView("UnitTestsRecoveryView", "i1");
      checkView("UnitTestsRecoveryView2", "i2");
      checkView("UnitTestsRecoveryView3", "i3");
      checkView("UnitTestsRecoveryView4", "i1");
      checkView("UnitTestsRecoveryView5", "i2");

      let checkIndex = function(indexName, analyzer, includeAllFields, hasFields) {
        let c = db._collection("UnitTestsRecoveryDummy");
        let indexes = c.getIndexes().filter(i => i.type === "inverted" && i.name === indexName);
        assertEqual(1, indexes.length);

        let i = indexes[0];
        assertEqual(includeAllFields, i.includeAllFields);
        assertEqual(analyzer, i.analyzer);
        let fields = i.fields;
        if (hasFields) {
          assertEqual(3, fields.length);
          let fieldA = fields.find(f => f.name === "a");
          assertEqual("a", fieldA.name);
          let fieldB = fields.find(f => f.name === "b");
          assertEqual("b", fieldB.name);
          let fieldC = fields.find(f => f.name === "c");
          assertEqual("c", fieldC.name);
        } else {
          assertEqual(0, fields.length);
        }
      };


      checkIndex("i1", "identity", false, true);
      checkIndex("i2", "calcAnalyzer", true, true);
      checkIndex("i3", "identity", true, false);

      let expectedCount = db._query("FOR doc IN UnitTestsRecoveryDummy FILTER doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray()[0];

      let queries = [
        "FOR doc IN UnitTestsRecoveryView SEARCH doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length",
        "FOR doc IN UnitTestsRecoveryView2 SEARCH doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length",
        "FOR doc IN UnitTestsRecoveryView3 SEARCH doc.c >= 0 AND STARTS_WITH(doc._id, 'UnitTestsRecoveryDummy') COLLECT WITH COUNT INTO length RETURN length",
        "FOR doc IN UnitTestsRecoveryView4 SEARCH doc.c >= 0 OPTIONS {waitFoSync:true} COLLECT WITH COUNT INTO length RETURN length",
        "FOR doc IN UnitTestsRecoveryView5 SEARCH doc.c >= 0 || STARTS_WITH(doc._id, 'UnitTestsRecoveryDummy') COLLECT WITH COUNT INTO length RETURN length"
      ];

      queries.forEach(q => assertEqual(expectedCount, db._query(q).toArray()[0]));
    }
  };
}

function main (argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.writeDone().status ? 0 : 1;
  }
}
