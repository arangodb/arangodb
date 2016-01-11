/*jshint globalstrict:false, strict:false */
/*global assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the performance of removal with a skip-list index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function SkipListPerfSuite() {
  'use strict';
  var cn = "UnitTestsCollectionSkiplistPerf";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  setUp : function () {
    internal.db._drop(cn);
    collection = internal.db._create(cn, { waitForSync : false });
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  tearDown : function () {
    // try...catch is necessary as some tests delete the collection itself!
    try {
      collection.unload();
      collection.drop();
    }
    catch (err) {
    }

    collection = null;
    internal.wait(0.0);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: performance of deletion with skip-list index
////////////////////////////////////////////////////////////////////////////////

    testDeletionPerformance : function () {
      var time = require("internal").time;
      collection.ensureSkiplist("value");
      var N=100000;
      var p=14777;  // must be coprime to N
      for (i = 0;i < N;i++) {
        collection.save({value:i});
      }
      var l = collection.toArray();
      var t = time();
      var j = 0;
      var x;
      for (var i = 0;i < l.length;i++) {
        x = l[j];
        j = (j+p) % l.length;
        collection.remove(x._key);
      }
      var t1 = time()-t;
      internal.db._drop(cn);
      collection = internal.db._create(cn);
      collection.ensureSkiplist("value");
      for (i = 0;i < N;i++) {
        collection.save({value: i % 10});
      }
      l = collection.toArray();
      t = time();
      j = 0;
      for (i = 0;i < l.length;i++) {
        x = l[j];
        j = (j+p) % l.length;
        collection.remove(x._key);
      }
      var t2 = time()-t;
      assertFalse(t2 > 5*t1,"Removal with skip-list index is slow");
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SkipListPerfSuite);

return jsunity.done();


