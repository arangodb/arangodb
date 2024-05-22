/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertMatch */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangodDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

function EngineStatsSuite() {
  'use strict';
  const internal = require("internal");
  const db = require("@arangodb").db;

  const validateObject = function(stat) {
    assertTrue(typeof stat === 'object');
    // check for the existence of some key attributes in the data
    assertTrue(stat.hasOwnProperty("rocksdb.block-cache-capacity"));
    assertTrue(stat.hasOwnProperty("rocksdb.block-cache-usage"));
    assertTrue(stat.hasOwnProperty("cache.limit"));
    assertTrue(stat.hasOwnProperty("cache.allocated"));
    assertTrue(stat.hasOwnProperty("columnFamilies"));
  };
  
  return {
    testStats: function () {
      let stats = db._engineStats();

      if (internal.isCluster()) {
        let keys = Object.keys(stats);
        assertTrue(keys.length > 1, keys);
        keys.forEach(function(key) {
          assertMatch(/^PRMR/, key);
          validateObject(stats[key]);
        });
      } else {
        validateObject(stats);
      }
    },
  }; 
} 

jsunity.run(EngineStatsSuite);

return jsunity.done();
