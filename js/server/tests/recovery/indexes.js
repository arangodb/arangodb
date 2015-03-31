/*jshint globalstrict: false, unused : false */
/*global require, assertEqual, assertFalse */
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

var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  "use strict";
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;

  c.ensureHashIndex("value1");
  c.ensureSkiplist("value2");
  c.ensureCapConstraint(5000);

  for (i = 0; i < 1000; ++i) {
    c.save({ _key: "test" + i, value1: i, value2: "test" + (i % 10) });
  }

  c.save({ _key: "crashme" }, true);

  internal.debugSegfault("crashing server");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  "use strict";
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
    },
    tearDown: function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether we can restore the trx data
////////////////////////////////////////////////////////////////////////////////
    
    testIndexes : function () {
      var docs;
      var c = db._collection("UnitTestsRecovery"), i;
      var idx = c.getIndexes().sort(function (l, r) {
        if (l.id.length !== r.id.length) {
          return l.id.length - r.id.length < 0 ? -1 : 1;
        }

        // length is equal
        for (var i = 0; i < l.id.length; ++i) {
          if (l.id[i] !== r.id[i]) {
            return l.id[i] < r.id[i] ? -1 : 1;
          }
        }

        return 0;
      });

      assertEqual(4, idx.length);

      assertEqual("primary", idx[0].type);
      assertEqual("hash", idx[1].type);
      assertFalse(idx[1].unique);
      assertFalse(idx[1].sparse);
      assertEqual([ "value1" ], idx[1].fields);
      var hash = idx[1];
      
      assertEqual("skiplist", idx[2].type);
      assertFalse(idx[2].unique);
      assertFalse(idx[2].sparse);
      assertEqual([ "value2" ], idx[2].fields);
      var skip = idx[2];
      
      assertEqual("cap", idx[3].type);
      assertEqual(5000, idx[3].size);
      assertEqual(0, idx[3].byteSize);

      for (i = 0; i < 1000; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i, doc.value1); 
        assertEqual("test" + (i % 10), doc.value2); 
      }

      for (i = 0; i < 1000; ++i) {
        docs = c.byExampleHash(hash.id, { value1: i }).toArray();
        assertEqual(1, docs.length);
        assertEqual("test" + i, docs[0]._key);
      }

      for (i = 0; i < 1000; ++i) {
        docs = c.byExampleSkiplist(skip.id, { value2: "test" + (i % 10) }).toArray();
        assertEqual(100, docs.length);
      }

    }
        
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  "use strict";
  if (argv[1] === "setup") {
    runSetup();
    return 0;
  }
  else {
    jsunity.run(recoverySuite);
    return jsunity.done().status ? 0 : 1;
  }
}

