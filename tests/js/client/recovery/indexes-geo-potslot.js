/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse, assertTrue, assertNotNull */
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

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecovery');
  var c = db._create('UnitTestsRecovery');
  c.ensureIndex({ "type" : "geo", "fields" : ["geoloc.lat", "geoloc.lon"] });
  
  for (var i = 0; i < 1000; ++i) {
    var a = Math.floor(Math.random() * 100000000);
    var b = Math.floor(Math.random() * 100000000);
   
    c.insert({ geoloc: { lat: a % 90, lon: b % 90 } });
  }
  c.save({ _key: 'crashme' }, true);

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

    testIndexesGeo: function () {
      var c = db._collection('UnitTestsRecovery');

      // update all documents in the collection. this will re-write the geo index 
      // entries. this simply should not fail
      db._query("FOR doc IN " + c.name() + " UPDATE doc WITH { modified: true } IN " + c.name()).toArray();
      assertEqual(1001, db._query("FOR doc IN " + c.name() + " FILTER doc.modified == true RETURN 1").toArray().length);
      
      // again update all documents in the collection. this will re-write the geo index 
      // entries. again this simply should not fail
      db._query("FOR doc IN " + c.name() + " UPDATE doc WITH { modified: false } IN " + c.name()).toArray();
      assertEqual(1001, db._query("FOR doc IN " + c.name() + " FILTER doc.modified == false RETURN 1").toArray().length);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
