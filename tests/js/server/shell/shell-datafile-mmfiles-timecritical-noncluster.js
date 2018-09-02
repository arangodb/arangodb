/*jshint globalstrict:false, strict:false, maxlen : 200 */
/*global fail, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for transactions
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

var jsunity = require("jsunity");
var internal = require("internal");
var arangodb = require("@arangodb");
var db = arangodb.db;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function datafileFailuresSuite () {
  'use strict';
  var cn = "UnitTestsDatafile";
  var c = null;
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.debugClearFailAt();

      if (c !== null) {
        c.drop();
      }

      c = null;
      internal.wait(4);
    },
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test: disk full
////////////////////////////////////////////////////////////////////////////////

    testDiskFullDuringLogfileCreationNoJournal : function () {
      ["CreateDatafile1", "CreateDatafile2"].forEach(function(what) {
        internal.debugClearFailAt();
        db._drop(cn);
     
        while (true) {
          try { 
            internal.wal.flush(true, true);
            break;
          }
          catch (err) {
          }
          internal.wait(0.5, false);
        } 
        c = db._create(cn);
      
        internal.debugSetFailAt(what);
        internal.debugSetFailAt("LogfileManagerGetWriteableLogfile");
        try {
          internal.wal.flush();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_NO_JOURNAL.code, err.errorNum);
        }

        internal.wait(3, false);
        internal.debugClearFailAt();
        assertEqual(0, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: disk full
////////////////////////////////////////////////////////////////////////////////

    testDiskFullDuringCollectionNoJournal : function () {
      ["CreateDatafile1", "CreateDatafile2"].forEach(function(what) {
        internal.debugClearFailAt();
        db._drop(cn);
     
        while (true) {
          try { 
            internal.wal.flush(true, true);
            break;
          }
          catch (err) {
          }
          internal.wait(0.5, false);
        } 

        c = db._create(cn);
        for (var i = 0; i < 1000; ++i) {
          c.insert({ value: i });
        }
      
        internal.debugSetFailAt(what);
        internal.debugSetFailAt("LogfileManagerGetWriteableLogfile");
        try {
          internal.wal.flush();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_NO_JOURNAL.code, err.errorNum);
        }
        
        internal.wait(3, false);
        internal.debugClearFailAt();
        assertEqual(1000, c.count());
      });
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

// only run this test suite if server-side failures are enabled
if (internal.debugCanUseFailAt()) {
  jsunity.run(datafileFailuresSuite);
}

return jsunity.done();


