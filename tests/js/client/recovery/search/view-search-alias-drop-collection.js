/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse */
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
/// @author Copyright 2023, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  db._drop('UnitTestsRecoveryDummy2');

  var c = db._create('UnitTestsRecoveryDummy');
  c.ensureIndex({ type: "inverted", name: "pupa", includeAllFields:true });
  var meta = { indexes: [ { collection: "UnitTestsRecoveryDummy", index: "pupa" } ] };

  db._dropView('UnitTestsRecovery1');
  var v = db._createView('UnitTestsRecovery1', 'search-alias', meta);

  c.drop();

  c = db._create('UnitTestsRecoveryDummy2');
  c.save({ name: 'crashme' }, true);

  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testIResearchLinkDropCollection: function () {
      let checkView = function(viewName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(0, indexes.length);
      };
      checkView("UnitTestsRecovery1");

      var result = db._query("FOR doc IN UnitTestsRecovery1 SEARCH doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray();
      assertEqual(0, result[0]);
    }
  };
}

jsunity.run(recoverySuite);
return jsunity.done();
