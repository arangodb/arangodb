/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for dump/reload
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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  var c, v, e, i;
  db._drop('UnitTestsVertices');
  db._drop('UnitTestsEdges');

  v = db._create('UnitTestsVertices');
  e = db._createEdgeCollection('UnitTestsEdges');

  for (i = 0; i < 1000; ++i) {
    v.save({ _key: 'node' + i, name: 'some-name' + i });
  }

  for (i = 0; i < 1000; ++i) {
    e.save('UnitTestsVertices/node' + i, 'UnitTestsVertices/node' + (i % 10),
      { _key: 'edge' + i, what: 'some-value' + i });
  }

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true); // wait for sync

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

    testEdges: function () {
      var v, e, doc, i;

      v = db._collection('UnitTestsVertices');
      assertEqual(1000, v.count());
      for (i = 0; i < 1000; ++i) {
        assertEqual('some-name' + i, v.document('node' + i).name);
      }

      e = db._collection('UnitTestsEdges');
      for (i = 0; i < 1000; ++i) {
        doc = e.document('edge' + i);
        assertEqual('some-value' + i, doc.what);
        assertEqual('UnitTestsVertices/node' + i, doc._from);
        assertEqual('UnitTestsVertices/node' + (i % 10), doc._to);
      }

      for (i = 0; i < 1000; ++i) {
        assertEqual(1, e.outEdges('UnitTestsVertices/node' + i).length);
        if (i >= 10) {
          assertEqual(0, e.inEdges('UnitTestsVertices/node' + i).length);
        } else {
          assertEqual(100, e.inEdges('UnitTestsVertices/node' + i).length);
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
    return jsunity.writeDone().status ? 0 : 1;
  }
}
