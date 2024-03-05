/* jshint strict: false, sub: true */
/* global print */
'use strict';

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const platform = require('internal').platform;

class tmpDirManager {
  constructor(testName, options) {
    this.tempDir = fs.join(fs.getTempPath(), testName);
    this.orgTempDir = process.env.TMPDIR;
    process.env.TMPDIR = this.tempDir;
    fs.makeDirectoryRecursive(this.tempDir);
    if (options.extremeVerbosity) {
      print("temporary directory now: " + this.tempDir);
    }
  }
  destructor(cleanup) {
    process.env.TMPDIR = this.orgTempDir;
    if (cleanup) {
      fs.removeDirectoryRecursive(this.tempDir, true);
    }
  }
}

exports.tmpDirManager = tmpDirManager;
