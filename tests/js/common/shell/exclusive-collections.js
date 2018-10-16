/*jshint globalstrict:false, strict:false */
/*global assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test exclusive collections
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
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;

function ExclusiveSuite () {
  const cn1 = "UnitTestsExclusiveCollection1"; 
  const cn2 = "UnitTestsExclusiveCollection2";

  return {

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._create(cn1);
      db._create(cn2);
    },

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
    },
    
    testReadOne : function () {
      db._executeTransaction({
        collections: { read: [ cn1 ] },
        action: function(params) {
        },
        params: { cn1 }
      });

      assertEqual(0, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },
    
    testReadTwo : function () {
      db._executeTransaction({
        collections: { read: [ cn1, cn2 ] },
        action: function(params) {
        },
        params: { cn1, cn2 }
      });

      assertEqual(0, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },
    
    testWriteOne : function () {
      db._executeTransaction({
        collections: { write: [ cn1 ] },
        action: function(params) {
          let db = require("@arangodb").db;
          db[params.cn1].insert({});
        },
        params: { cn1 }
      });

      assertEqual(1, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },

    testWriteTwo : function () {
      db._executeTransaction({
        collections: { write: [ cn1, cn2 ] },
        action: function(params) {
          let db = require("@arangodb").db;
          db[params.cn1].insert({});
          db[params.cn2].insert({});
        },
        params: { cn1, cn2 }
      });
      
      assertEqual(1, db[cn1].count());
      assertEqual(1, db[cn2].count());
    },
    
    testExclusiveOne : function () {
      db._executeTransaction({
        collections: { exclusive: [ cn1 ] },
        action: function(params) {
          let db = require("@arangodb").db;
          db[params.cn1].insert({});
        },
        params: { cn1 }
      });

      assertEqual(1, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },

    testExclusiveTwo : function () {
      try {
        db._executeTransaction({
          collections: { exclusive: [ cn1, cn2 ] },
          action: function(params) {
          },
          params: { cn1, cn2 }
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_MULTIPLE_EXCLUSIVE_COLLECTIONS.code, err.errorNum);
      }
      
      assertEqual(0, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },
    
    testWriteOneAql : function () {
      db._query("INSERT {} INTO " + cn1 + " OPTIONS {}");

      assertEqual(1, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },

    testWriteTwoAql : function () {
      db._query("INSERT {} INTO " + cn1 + " OPTIONS {} INSERT {} INTO " + cn2 + " OPTIONS {}");

      assertEqual(1, db[cn1].count());
      assertEqual(1, db[cn2].count());
    },
    
    testExclusiveOneAql : function () {
      db._query("INSERT {} INTO " + cn1 + " OPTIONS {exclusive: true} INSERT {} INTO " + cn2 + " OPTIONS {}");

      assertEqual(1, db[cn1].count());
      assertEqual(1, db[cn2].count());
    },

    testExclusiveTwoAql : function () {
      try {
        db._query("INSERT {} INTO " + cn1 + " OPTIONS {exclusive: true} INSERT {} INTO " + cn2 + " OPTIONS {exclusive: true}");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_TRANSACTION_MULTIPLE_EXCLUSIVE_COLLECTIONS.code, err.errorNum);
      }
      
      assertEqual(0, db[cn1].count());
      assertEqual(0, db[cn2].count());
    },
  
  };
}

jsunity.run(ExclusiveSuite);

return jsunity.done();
