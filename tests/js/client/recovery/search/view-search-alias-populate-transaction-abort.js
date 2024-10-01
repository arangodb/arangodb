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

var arangodb = require('@arangodb');
var db = arangodb.db;
var internal = require('internal');
var jsunity = require('jsunity');
var transactionFailure = require('@arangodb/test-helper-common').transactionFailure;

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  var c = db._create('UnitTestsRecoveryDummy');
  var i1 = c.ensureIndex({ type: "inverted", name: "pupa", includeAllFields: true });

  var meta = { indexes: [ { index: i1.name, collection: c.name() } ] };
  db._dropView('UnitTestsRecoveryView');
  db._createView('UnitTestsRecoveryView', 'search-alias', meta);

  return transactionFailure(
    {
      collections: {
        write: ['UnitTestsRecoveryDummy']
      },
      action: function() {
        var db = require('@arangodb').db;
        var c = db.UnitTestsRecoveryDummy;
        for (let i = 0; i < 10000; i++) {
          c.save({ a: "foo_" + i, b: "bar_" + i, c: i });
        }
        throw new Error('intentional abort');
      },
      waitForSync: true
    },
    internal.errors.ERROR_TRANSACTION_INTERNAL.code,
    'Error: intentional abort',
    true,
    false);
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testIResearchLinkPopulateTransactionAbort: function () {
      let checkView = function(viewName, indexName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(1, indexes.length);
        assertEqual(indexName, indexes[0].index);
        assertEqual("UnitTestsRecoveryDummy", indexes[0].collection);
      };

      checkView("UnitTestsRecoveryView", "pupa");

      let checkIndex = function(indexName, analyzer, includeAllFields, hasFields) {
        let c = db._collection("UnitTestsRecoveryDummy");
        let indexes = c.getIndexes().filter(i => i.type === "inverted" && i.name === indexName);
        assertEqual(1, indexes.length);

        let i = indexes[0];
        assertEqual(includeAllFields, i.includeAllFields);
        assertEqual(analyzer, i.analyzer);
      };

      checkIndex("pupa", "identity", true);

      var result = db._query("FOR doc IN UnitTestsRecoveryView SEARCH doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray();
      assertEqual(0, result[0]);
    }
  };
}

jsunity.run(recoverySuite);
return jsunity.done();
