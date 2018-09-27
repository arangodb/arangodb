/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue */

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');
var fs = require('fs');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecovery');
  var c = db._create('UnitTestsRecovery');
  var i, num = 9999999999;
  for (i = 0; i < 4; ++i) {
    var filename = fs.join(c.path(), 'datafile-' + num + '.db');
    num++;

    // save an empty file 
    fs.write(filename, '');
  }

  c.save({ test: 'testValue' }, true); // wait for sync

  internal.debugSegfault('crashing server');
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
    // / @brief test whether we can start the server
    // //////////////////////////////////////////////////////////////////////////////

    testEmptyDatafiles: function () {
      var c = db._collection('UnitTestsRecovery');
      assertTrue(1, c.count());
      assertEqual('testValue', c.toArray()[0].test);
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
    return jsunity.done().status ? 0 : 1;
  }
}
