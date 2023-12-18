/*jshint globalstrict:false, strict:false */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests unloading while there are still document barriers
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function UnloadSuite () {
  'use strict';
  var cn = "UnitTestsCollectionUnload";
  var collection;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);

      collection = internal.db._create(cn);

      collection.save({ _key: "test1" });
      collection.save({ _key: "test2" });
      collection.save({ _key: "test3" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unloading while there are still barriers
////////////////////////////////////////////////////////////////////////////////

    testUnload : function () {
      var doc1 = collection.document("test1");
      var doc2 = collection.document("test2");

      // note: the collection won't be unloaded as we hold barriers
      collection.unload();
      collection.unload();

      internal.wait(5);

      var doc3 = collection.document("test3");

      doc1 = null;
      collection.unload();
      doc2 = null;
      internal.print("waiting for unload");
      internal.wait(5);
      doc3 = null;

      collection.unload();
      collection.drop();
      collection = null;

      internal.wait(0.0);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UnloadSuite);

return jsunity.done();

