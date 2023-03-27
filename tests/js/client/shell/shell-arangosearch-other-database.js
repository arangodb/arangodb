/*jshint globalstrict:false, strict:false, maxlen:1000*/
/*global assertEqual, assertTrue, assertFalse, fail, more */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the statement class
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Wilfried Goesgens
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const queries = require('@arangodb/aql/queries');
const db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: statements
////////////////////////////////////////////////////////////////////////////////

function ViewsOtherDB () {
  'use strict';
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test cursor
////////////////////////////////////////////////////////////////////////////////

    testViewOtherDB : function () {
      const otherViwesDBN = 'otherViwesDB';
      try {
        db._createDatabase(otherViwesDBN);
        db._useDatabase(otherViwesDBN);
        let c = db._create("c",{numberOfShards:3});
        let v = db._createView("v", "arangosearch", {});
        v.properties({links:{c:{includeAllFields:true}}});
        let docs = [];
        for (let i = 0; i < 100; ++i) {
          docs.push({Hallo:i});
        }
        c.save(docs);
        db._query(`FOR d IN v OPTIONS { waitForSync:true } LIMIT 1 RETURN 1`);
        v.properties({links:{c:null}});
        v.drop();
        c.drop();
        db._useDatabase("_system");
        db._dropDatabase(otherViwesDBN);
        let dirs = global.instanceManager.getStrayDirectories();
        assertTrue(dirs.length === 0, `views directories not cleaned up: ${JSON.stringify(dirs)}`);
      } finally {
        db._useDatabase('_system');
        try {
          db._dropDatabase(otherViwesDBN);
        } catch (e) {} 
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ViewsOtherDB);
return jsunity.done();

