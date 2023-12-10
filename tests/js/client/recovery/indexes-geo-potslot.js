/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue, assertNotNull */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for transactions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecovery');
  var c = db._create('UnitTestsRecovery');
  c.ensureIndex({ "type" : "geo", "fields" : ["geoloc.lat", "geoloc.lon"] });
  
  for (var i = 0; i < 1000; ++i) {
    var a = Math.floor(Math.random() * 100000000);
    var b = Math.floor(Math.random() * 100000000);
   
    c.insert({ geoloc: { lat: a % 90, lon: b % 90 } });
  }
  c.save({ _key: 'crashme' }, true);

  internal.debugTerminate('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {},
    tearDown: function () {},

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
