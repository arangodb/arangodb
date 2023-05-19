/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'rocksdb.auto-refill-index-caches-on-modify' : 'true',
    'rocksdb.auto-refill-index-caches-on-followers' : 'false',
    'server.statistics' : 'false',
    'foxx.queues' : 'false',
  };
}

const db = require('@arangodb').db;
const jsunity = require('jsunity');
const errors = require('internal').errors;

let { getEndpointsByType,
      debugCanUseFailAt,
      debugSetFailAt,
      debugClearFailAt,
    } = require('@arangodb/test-helper');

const cn = 'UnitTestsCollection';

function AutoRefillIndexCachesOnFollowers() {
  'use strict';

  let c;

  return {
    setUpAll: function() {
      c = db._create(cn, { replicationFactor: 2 });
    },

    tearDown: function() {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      try {
        c.remove("test");
      } catch (err) {}
    },

    tearDownAll: function() {
      db._drop(cn);
    },
    
    testInsertDefaultOk: function() {
      getEndpointsByType("dbserver").forEach((ep) => debugSetFailAt(ep, "RefillIndexCacheOnFollowers::failIfTrue"));
      // insert should just work
      c.insert({_key: "test"});
      assertEqual(1, c.count());
    },
    
    testInsertRequestedOk: function() {
      getEndpointsByType("dbserver").forEach((ep) => debugSetFailAt(ep, "RefillIndexCacheOnFollowers::failIfTrue"));
      // insert should just work
      c.insert({_key: "test"}, { refillIndexCaches: true });
      assertEqual(1, c.count());
    },
    
    testInsertOptOutOk: function() {
      getEndpointsByType("dbserver").forEach((ep) => debugSetFailAt(ep, "RefillIndexCacheOnFollowers::failIfTrue"));
      // insert should just work
      c.insert({_key: "test"}, { refillIndexCaches: false });
      assertEqual(1, c.count());
    },
    
  };
}

let ep = getEndpointsByType('dbserver');
if (ep && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(AutoRefillIndexCachesOnFollowers);
}
return jsunity.done();
