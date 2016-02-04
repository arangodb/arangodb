/*jshint strict: false, sub: true */
/*global db */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2014 triagens GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////


// Call like this: arangosh --javascript.execute scripts/read_mimetypes.js


var fs = require("fs");
var print = require("internal").print;
var mimetypes = fs.readBuffer(fs.pathSeparator + fs.join("etc", "mime.types"));

var mt;
if (!db.hasOwnProperty('mimeTypes')) {
  mt = db._create("mimeTypes");
  mt.ensureIndex({ type: "skiplist", fields: [ "extension" ] });
}
else {
  mt = db.mimeTypes;
}

let maxBuffer = mimetypes.length;
let lineStart = 0;
for (let j = 0; j < maxBuffer; j++) {
  if (mimetypes[j] === 10) { // \n
    const line = mimetypes.asciiSlice(lineStart, j);
    lineStart = j + 1;
    if ((line[0] === '#') || (line.length === 0)) {
      continue;
    }
    // replace multiple blanks by one:
    var lineSegments = line.split(/[\s\t]+/);
    if (lineSegments.length === 1) {
      continue; // Skip lines that just have a mimetype.
    }
    for (var i = 1; i < lineSegments.length; i++) {
      mt.save({extension: lineSegments[i], mimeType: lineSegments[0]});      
    }
  }
}

print("Saved " + mt.count() + " mimetypes.");
