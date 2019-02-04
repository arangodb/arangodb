/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertTrue, assertEqual, assertNotEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test ttl configuration
///
/// @file
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const ERRORS = arangodb.errors;
const globalProperties = internal.ttlProperties();

function TtlSuite () {
  'use strict';
  const cn = "UnitTestsTtl";

  return {

    setUp : function () {
      db._drop(cn);
      internal.ttlProperties(globalProperties);
    },

    tearDown : function () {
      db._drop(cn);
      internal.ttlProperties(globalProperties);
    },
    
    testStatistics : function () {
      let stats = internal.ttlStatistics();
      assertTrue(stats.hasOwnProperty("runs"));
      assertEqual("number", typeof stats.runs);
      assertTrue(stats.hasOwnProperty("documentsRemoved"));
      assertEqual("number", typeof stats.documentsRemoved);
      assertTrue(stats.hasOwnProperty("limitReached"));
      assertEqual("number", typeof stats.limitReached);
    },
    
    testProperties : function () {
      let properties = internal.ttlProperties();
      assertTrue(properties.hasOwnProperty("active"));
      assertEqual("boolean", typeof properties.active);
      assertTrue(properties.hasOwnProperty("frequency"));
      assertEqual("number", typeof properties.frequency);
      assertTrue(properties.hasOwnProperty("maxCollectionRemoves"));
      assertEqual("number", typeof properties.maxCollectionRemoves);
      assertTrue(properties.hasOwnProperty("maxTotalRemoves"));
      assertEqual("number", typeof properties.maxTotalRemoves);
      assertTrue(properties.hasOwnProperty("onlyLoadedCollections"));
      assertEqual("boolean", typeof properties.onlyLoadedCollections);
    },
    
    testSetProperties : function () {
      let values = [
        [ { active: null }, false ],
        [ { active: "foo" }, false ],
        [ { active: 404 }, false ],
        [ { active: false }, true ],
        [ { active: true }, true ],
        [ { frequency: "foobar" }, false ],
        [ { frequency: null }, false ],
        [ { frequency: 1000 }, true ],
        [ { frequency: 100000 }, true ],
        [ { maxCollectionRemoves: true }, false ],
        [ { maxCollectionRemoves: "1000" }, false ],
        [ { maxCollectionRemoves: 1000 }, true ],
        [ { maxCollectionRemoves: 100000 }, true ],
        [ { maxTotalRemoves: true }, false ],
        [ { maxTotalRemoves: "5000" }, false ],
        [ { maxTotalRemoves: 100 }, true ],
        [ { maxTotalRemoves: 5000 }, true ],
        [ { maxTotalRemoves: 500000 }, true ],
        [ { onlyLoadedCollections: null }, false ],
        [ { onlyLoadedCollections: false }, true ],
        [ { onlyLoadedCollections: true }, true ],
        [ { onlyLoadedCollections: 1234 }, false ],
        [ { onlyLoadedCollections: "foo" }, false ],
        [ { onlyLoadedCollections: "foo" }, false ],
      ];

      values.forEach(function(value) {
        if (value[1]) {
          let properties = internal.ttlProperties(value[0]);
          let keys = Object.keys(value[0]);
          keys.forEach(function(k) {
            assertEqual(properties[k], value[0][k]);
          });
          properties = internal.ttlProperties();
          keys.forEach(function(k) {
            assertEqual(properties[k], value[0][k]);
          });
        } else {
          let properties = internal.ttlProperties();
          try {
            internal.ttlProperties(value[0]);
            fail();
          } catch (err) {
            assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code, value);
          }
          let properties2 = internal.ttlProperties();
          let keys = Object.keys(value[0]);
          keys.forEach(function(k) {
            assertEqual(properties[k], properties2[k]);
            assertNotEqual(properties[k], value[0][k]);
          });
        }
      });
    },
    
    testActive : function () {
      // disable first
      internal.ttlProperties({ active: false });

      let stats = internal.ttlStatistics();
      const oldRuns = stats.runs;
      
      // reenable first
      internal.ttlProperties({ active: true, frequency: 1000 });

      let tries = 0;
      while (tries++ < 10) {
        internal.wait(1, false);
      
        stats = internal.ttlStatistics();
        if (stats.runs > oldRuns) {
          break;
        }
      }

      // wait for the number of runs to increase
      assertTrue(stats.runs > oldRuns);
    },
    
    testInactive : function () {
      // disable first
      internal.ttlProperties({ active: false });

      let stats = internal.ttlStatistics();
      const oldRuns = stats.runs;
      
      // reenable first
      internal.ttlProperties({ active: false, frequency: 1000 });

      let tries = 0;
      while (tries++ < 10) {
        internal.wait(1, false);
      
        stats = internal.ttlStatistics();
        if (stats.runs > oldRuns) {
          break;
        }
      }

      // number of runs must not have changed
      assertEqual(stats.runs, oldRuns);
    },


    /*
    testInvalidKeyGenerator : function () {
      try {
        db._create(cn, { keyOptions: { type: "der-fuchs" } });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_INVALID_KEY_GENERATOR.code, err.errorNum);
      }
    },
*/
  };
}

jsunity.run(TtlSuite);

return jsunity.done();
