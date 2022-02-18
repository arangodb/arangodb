/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertEqual, db*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hotbackup in single server
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
const internal = require("internal");

'use strict';

const dbName = "UnitTestData";

function dirmaker(n, withData) {
  db._useDatabase("_system");
  db._createDatabase(n);
  db._useDatabase(n);
  let c = db._create("c",{numberOfShards:3});
  let v = db._createView("v", "arangosearch", {});
  v.properties({links:{c:{includeAllFields:true}}});
  for (let i = 0; i < 100; ++i) {
    c.insert({Hallo:i});
  }
  db._query(`FOR d IN v OPTIONS { waitForSync:true } LIMIT 1 RETURN 1`);
  if (!withData) {
    v.properties({links:{c:null}});
    v.drop();
    c.drop();
  }
  db._useDatabase("_system");
  db._dropDatabase(n);
}

function createSomeData() {
  db._useDatabase("_system");
  dirmaker(dbName + "1", false);
  dirmaker(dbName + "2", true);
  db._createDatabase(dbName);
  db._useDatabase(dbName);
  let c = db._create("c",{numberOfShards:3});
  let v = db._createView("v", "arangosearch", {});
  v.properties({links:{c:{includeAllFields:true}}});
  let l = [];
  for (let i = 0; i < 10000; ++i) {
    l.push({Hallo:i}); 
    if (l.length % 10000 === 0) {
      c.insert(l);
      l = [];
    }
  }
  db._query(`FOR d IN v OPTIONS { waitForSync:true } LIMIT 1 RETURN 1`);
  db._useDatabase("_system");
}

function dropData() {
  db._useDatabase("_system");
  db._dropDatabase(dbName);
}

function HotBackupSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// Setup
////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      createSomeData();
    },

////////////////////////////////////////////////////////////////////////////////
/// Teardown
////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      dropData();
    },

////////////////////////////////////////////////////////////////////////////////
/// Check sums and counts in META after local upload
////////////////////////////////////////////////////////////////////////////////

    testCheckSumsAndCounts: function() {
      let fs = require("fs");
      let tempPath = fs.getTempPath();
      console.warn("Temporary path:", tempPath);
      let backup = arango.POST("/_admin/backup/create", {}).result;
      let upload  = arango.POST("/_admin/backup/upload",
        {"id":backup.id,"remoteRepository":"local:" + tempPath,
         "config":{"local":{"type":"local","copy-links":"false",
                            "links":"false","one_file_system":"true"}}}
      ).result;
      while (true) {
        let progress = arango.POST("/_admin/backup/upload", 
          {uploadId: upload.uploadId}).result;
        console.warn(JSON.stringify(progress));
        if (progress.DBServers.SNGL.Status === "COMPLETED") {
          break;
        }
        internal.wait(1);
      }
      let backupPath = fs.join(tempPath, backup.id);
      let META = JSON.parse(fs.readFileSync(fs.join(backupPath, "META")));
      assertTrue(META.countIncludesFilesOnly);
      let tree = fs.listTree(backupPath);
      let totalSize = 0;
      let totalNumber = 0;
      for (let f of tree) {
        let full = fs.join(backupPath, f);
        if (fs.isFile(full) && f !== "META") {
          totalNumber += 1;
          totalSize += fs.size(full);
        }
      }
      assertEqual(totalSize, META.sizeInBytes);
      assertEqual(totalNumber, META.nrFiles);
      fs.removeDirectoryRecursive(backupPath);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(HotBackupSuite);

return jsunity.done();

