/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let db = arangodb.db;

function debugCanUseFailAt() {
  let res = arango.GET('/_admin/debug/failat');
  return res === true;
}

/// @brief set failure point
function debugSetFailAt(failAt) {
  let res = arango.PUT('/_admin/debug/failat/' + encodeURIComponent(failAt), {});
  if (res !== true) {
    throw "Error setting failure point";
  }
}

function debugClearFailAt(endpoint) {
  let res = arango.DELETE('/_admin/debug/failat');
  if (res !== true) {
    throw "Error removing failure points";
  }
}

function quickKeysSuite() {
  'use strict';
  const cn = 'UnitTestsQuickKeys';

  let createCollection = function(n) {
    let c = db._create(cn);
    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({ _key: "test" + i });
    }
    c.insert(docs);
  };

  let runTestForCount = function(n, quick, adjustQuickLimit) {
    debugSetFailAt("disableRevisionsAsDocumentIds");
    createCollection(n);
    
    let quickLimit = 1000000;
    if (adjustQuickLimit) {
      debugSetFailAt("RocksDBRestReplicationHandler::quickKeysNumDocsLimit100");
      quickLimit = 100;
    }

    let batch = arango.POST('/_api/replication/batch', {});
    try {
      let url = '/_api/replication/keys?collection=' + encodeURIComponent(cn) + '&batchId=' + encodeURIComponent(batch.id);
      if (quick) {
        url += '&quick=true';
      }
      let keys = arango.POST(url, {}); 
      if (n >= quickLimit && quick) {
        assertFalse(keys.hasOwnProperty('id'));
      } else {
        assertTrue(keys.hasOwnProperty('id'));
      }
      assertTrue(keys.hasOwnProperty('count'));
      assertEqual(n, keys.count);
    } finally {
      arango.DELETE('/_api/replication/batch/' + encodeURIComponent(batch.id));
    }
  };

  return {

    setUp: function () {
      debugClearFailAt();
      db._drop(cn);
    },

    tearDown: function () {
      debugClearFailAt();
      db._drop(cn);
    },
    
    testKeys0: function () {
      runTestForCount(0, false, false);
    },
    
    testKeys1000: function () {
      runTestForCount(1000, false, false);
    },
    
    testKeys5000: function () {
      runTestForCount(5000, false, false);
    },
    
    testKeys0WithLowQuickLimit: function () {
      runTestForCount(0, false, true);
    },
    
    testKeys1000WithLowQuickLimit: function () {
      runTestForCount(1000, false, true);
    },
    
    testKeys5000WithLowQuickLimit: function () {
      runTestForCount(5000, false, true);
    },
    
    testKeys0WithQuick: function () {
      runTestForCount(0, true, false);
    },
    
    testKeys1000WithQuick: function () {
      runTestForCount(1000, true, false);
    },
    
    testKeys5000WithQuick: function () {
      runTestForCount(5000, true, false);
    },
    
    testKeys0WithQuickWithLowQuickLimit: function () {
      runTestForCount(0, true, true);
    },
    
    testKeys1000WithQuickWithLowQuickLimit: function () {
      runTestForCount(1000, true, true);
    },
    
    testKeys5000WithQuickWithLowQuickLimit: function () {
      runTestForCount(5000, true, true);
    },
  };
}

if (debugCanUseFailAt()) {
  // only execute if failure tests are available
  jsunity.run(quickKeysSuite);
}
return jsunity.done();
