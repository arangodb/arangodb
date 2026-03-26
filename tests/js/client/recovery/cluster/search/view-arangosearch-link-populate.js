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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');
var analyzers = require("@arangodb/analyzers");

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  var c = db._create('UnitTestsRecoveryDummy');

  db._dropView('UnitTestsRecoveryView');
  db._dropView('UnitTestsRecoveryView2');
  try { analyzers.remove('calcAnalyzer', true); } catch(e) {}
  db._createView('UnitTestsRecoveryView', 'arangosearch', {});
  db._createView('UnitTestsRecoveryView2', 'arangosearch', {});
  db._createView('UnitTestsRecoveryView3', 'arangosearch', {});
  db._createView('UnitTestsRecoveryView4', 'arangosearch', {});
  db._createView('UnitTestsRecoveryView5', 'arangosearch', {});
  analyzers.save('calcAnalyzer',"aql",{queryString:"RETURN SOUNDEX(@param)"});

  var meta = { links: { 'UnitTestsRecoveryDummy': { includeAllFields: true } } };
  db._view('UnitTestsRecoveryView').properties(meta);
  var meta2 = { links: { 'UnitTestsRecoveryDummy': { includeAllFields: true, analyzers:['calcAnalyzer'] } } };
  db._view('UnitTestsRecoveryView2').properties(meta2);
  db._view('UnitTestsRecoveryView3').properties(meta);
  db._view('UnitTestsRecoveryView4').properties(meta);
  db._view('UnitTestsRecoveryView5').properties(meta);

  for (let i = 0; i < 10000; i++) {
    c.save({ a: "foo_" + i, b: "bar_" + i, c: i });
  }

  c.save({ name: 'crashme' }, { waitForSync: true });

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testIResearchLinkPopulate: function () {
      var v = db._view('UnitTestsRecoveryView');
      assertEqual(v.name(), 'UnitTestsRecoveryView');
      assertEqual(v.type(), 'arangosearch');
      var p = v.properties().links;
      assertTrue(p.hasOwnProperty('UnitTestsRecoveryDummy'));
      assertTrue(p.UnitTestsRecoveryDummy.includeAllFields);
      
      var v2 = db._view('UnitTestsRecoveryView2');
      var p2 = v2.properties().links;
      assertTrue(p2.hasOwnProperty('UnitTestsRecoveryDummy'));
      assertTrue(p2.UnitTestsRecoveryDummy.includeAllFields);
      assertEqual(p2.UnitTestsRecoveryDummy.analyzers, ['calcAnalyzer']);


      var expectedResult = db._query("FOR doc IN UnitTestsRecoveryDummy FILTER doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray();

      let result = db._query("FOR doc IN UnitTestsRecoveryView SEARCH doc.c >= 0 AND STARTS_WITH(doc._id, 'UnitTestsRecoveryDummy') "
                              + "OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length").toArray();
      let result2 = db._query("FOR doc IN UnitTestsRecoveryView2 SEARCH doc.c >= 0 OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length").toArray();
      let result3 = db._query("FOR doc IN UnitTestsRecoveryView3 SEARCH doc.c >= 0 AND STARTS_WITH(doc._id, 'UnitTestsRecoveryDummy') "
                              + "OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length").toArray();
      let result4 = db._query("FOR doc IN UnitTestsRecoveryView4 SEARCH doc.c >= 0 AND STARTS_WITH(doc._id, 'UnitTestsRecoveryDummy') "
                              + "OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length").toArray();
      let result5 = db._query("FOR doc IN UnitTestsRecoveryView5 SEARCH doc.c >= 0 AND STARTS_WITH(doc._id, 'UnitTestsRecoveryDummy') "
                              + "OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length").toArray();

      assertEqual(result[0], expectedResult[0]);
      assertEqual(result2[0], expectedResult[0]);
      assertEqual(result3[0], expectedResult[0]);
      assertEqual(result4[0], expectedResult[0]);
      assertEqual(result5[0], expectedResult[0]);

    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
