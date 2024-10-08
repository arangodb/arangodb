/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertTrue, assertFalse, assertNull, fail */
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
/// @author Andrey Abramov
/// @author Copyright 2022, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  var c = db._create('UnitTestsRecoveryDummy');
  var i1 = c.ensureIndex({ type: "inverted", name: "i1", includeAllFields:true });

  var meta = { indexes: [ { index: i1.name, collection: c.name() } ] };
  db._dropView('UnitTestsRecoveryWithLink');
  db._createView('UnitTestsRecoveryWithLink', 'search-alias', {});
  // store link
  db._view('UnitTestsRecoveryWithLink').properties(meta);

  c.save({ name: 'crashme' }, true);

  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testIResearchLinkCreate: function () {
      let checkView = function(viewName, indexName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(1, indexes.length);
        assertEqual(indexName, indexes[0].index);
        assertEqual("UnitTestsRecoveryDummy", indexes[0].collection);
      };
      checkView("UnitTestsRecoveryWithLink", "i1");

      let checkIndex = function(indexName, analyzer, includeAllFields) {
        let c = db._collection("UnitTestsRecoveryDummy");
        let indexes = c.getIndexes().filter(i => i.type === "inverted" && i.name === indexName);
        assertEqual(1, indexes.length);

        let i = indexes[0];
        assertEqual(includeAllFields, i.includeAllFields);
        assertEqual(analyzer, i.analyzer);
        let fields = i.fields;
      };

      checkIndex("i1", "identity", true);
    }
  };
}

jsunity.run(recoverySuite);
return jsunity.done();
