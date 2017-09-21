/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse */
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
  var c = db._create('UnitTestsRecovery'), i, fig;

  for (i = 0; i < 1000; ++i) {
    c.save({ _key: 'test' + i, value1: 'test' + i, value2: i });
  }

  internal.wal.flush(true, true);
 
  // wait until journal appears 
  while (true) {
    fig = c.figures();
    if (fig.journals.count > 0) {
      break;
    }
    internal.wait(1, false);
  }

  internal.debugSetFailAt('CreateMultipleJournals');

  c.rotate();

  for (i = 200; i < 400; ++i) {
    c.remove('test' + i);
  }
  for (i = 500; i < 700; ++i) {
    c.update('test' + i, { value2: 'test' + i });
  }

  internal.wal.flush(true, true);
  
  // wait until next journal appears 
  while (true) {
    fig = c.figures();
    if (fig.journals.count > 0) {
      break;
    }
    internal.wait(1, false);
  }
  
  c.rotate();

  var fs = require("fs"), journals = 0;
  fs.listTree(c.path()).forEach(function(filename) {
    if (filename.match(/journal-/)) {
      ++journals;
    }
  });

  if (journals < 2) {
    // we should have two journals when we get here
    // if not, we crash the server prematurely so the asserts in
    // the second phase will fail
    internal.debugSegfault('crashing server - expectation failed');
  }

  for (i = 200; i < 300; ++i) {
    c.insert({ _key: 'test' + i, value: 'test' + i });
  }
  for (i = 0; i < 100; ++i) {
    c.update('test' + i, { value: 'test' + i });
  }
  c.remove('test0');

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
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testMultipleJournals3: function () {
      var i, c = db._collection('UnitTestsRecovery'), doc;

      assertEqual(899, c.count());
      for (i = 0; i < 1000; ++i) {
        if (i === 0 || (i >= 300 && i < 400)) {
          assertFalse(c.exists('test' + i));
        } else {
          doc = c.document('test' + i);
          assertEqual('test' + i, doc._key);
          if (i > 0 && i < 100) {
            assertEqual('test' + i, doc.value);
            assertEqual('test' + i, doc.value1);
            assertEqual(i, doc.value2);
          } else if (i >= 200 && i < 300) {
            assertEqual('test' + i, doc.value);
            assertFalse(doc.hasOwnProperty('value1'));
            assertFalse(doc.hasOwnProperty('value2'));
          } else if (i >= 500 && i < 700) {
            assertFalse(doc.hasOwnProperty('value'));
            assertEqual('test' + i, doc.value1);
            assertEqual('test' + i, doc.value2);
          } else {
            assertFalse(doc.hasOwnProperty('value'));
            assertEqual('test' + i, doc.value1);
            assertEqual(i, doc.value2);
          }
        }
      }
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
