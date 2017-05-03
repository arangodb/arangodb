/*jshint globalstrict:false, strict:false */
/*global assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var internal = require("internal");
var fs = require("fs");

var db = arangodb.db;

function CollectionFilesSuite() {
  'use strict';
  var c = null;
  var cn = "UnitTestsCollectionFiles";

  return {
    setUp: function() {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    testFiles: function() {
      var path = c.path();
      assertTrue(fs.isDirectory(path));
      var files = fs.listTree(path);

      // "" is contained in list of files, so we check for > 1
      assertTrue(files.length > 1);

      c.drop();
      c = null;

      var tries = 0;

      // wait until directory has been removed
      while (++tries < 30) {
        internal.wait(1.0, true);
        
        if (!fs.isDirectory(path)) {
          // directory removed
          return;
        }
      }

      assertTrue(false);
    }
  };
}

jsunity.run(CollectionFilesSuite);
return jsunity.done();
