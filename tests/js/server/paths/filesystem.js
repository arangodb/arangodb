/*jshint globalstrict:false, strict:false */
/* global fail, getOptions, assertTrue, assertFalse, assertEqual, assertNotEqual, assertUndefined */

////////////////////////////////////////////////////////////////////////////////
/// @brief teardown for dump/reload tests
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const internal = require('internal');

let rootDir = fs.join(fs.getTempPath(), '..');
let testFilesDir = rootDir;

const topLevelAllowed = fs.join(testFilesDir, 'allowedカタログ');
const topLevelAllowedUnZip = fs.join(testFilesDir, 'allowed_unzüp');
const topLevelAllowedRecursive = fs.join(testFilesDir, 'allowed_recörsüve');
const allowedZipFileName = fs.join(topLevelAllowedRecursive, 'ällüüdd.zip');

const topLevelAllowedFile = fs.join(topLevelAllowed, 'allowed.txt');
const topLevelAllowedReadCSVFile = fs.join(topLevelAllowed, 'allowed_cßv.txt');
const topLevelAllowedReadJSONFile = fs.join(topLevelAllowed, 'allowed_json.txt');

var jsunity = require('jsunity');
var arangodb = require("@arangodb");

const base64Encode = require('internal').base64Encode;
function testSuite() {
  function tryReadAllowed(fn, expectedContent) {
    let content = fs.read(fn);
    assertEqual(content,
                expectedContent,
                'Expected ' + fn + ' to contain "' + expectedContent + '" but it contained: "' + content + '"!');
  }
  function tryReadBufferAllowed(fn, expectedContent) {
    let content = fs.readBuffer(fn);
    assertEqual(content.toString(),
                expectedContent,
                'Expected ' + fn + ' to contain "' + expectedContent + '" but it contained: "' + content + '"!');
  }
  function tryRead64Allowed(fn, expectedContentPlain) {
    let expectedContent = base64Encode(expectedContentPlain);
    let content = fs.read64(fn);
    assertEqual(content,
                expectedContent,
                'Expected ' + fn + ' to contain "' + expectedContent + '" but it contained: "' + content + '"!');
  }
  function tryReadCSVAllowed(fn) {
    let CSVContent = [];
    let content = internal.processCsvFile(fn, function (raw_row, index) {
      CSVContent.push(raw_row);
    });

    assertEqual(CSVContent,
                CSVParsed,
                'Expected ' + fn + ' to contain "' + CSVParsed + '" but it contained: "' + CSVContent + '"!');
  }
  function tryReadJSONAllowed(fn, expectedContentPlain) {
    let count = 0;
    let content = internal.processJsonFile(fn, function (raw_row, index) {
      assertEqual(raw_row,
                  JSONParsed,
                  'Expected ' + fn + ' to contain "' + JSONParsed + '" but it contained: "' + raw_row + '"!');
      count ++;
    });
    assertEqual(count, 2);
  }
  function tryAdler32Allowed(fn, expectedContentPlain) {
    let content = fs.adler32(fn);
    assertTrue(content > 0);
  }
  function tryWriteAllowed(fn, text) {
    let rc = fs.write(fn, text);
    assertTrue(rc, 'Expected ' + fn + ' to be writeable');
  }

  function tryAppendAllowed(fn, text) {
    let rc = fs.append(fn, text);
    assertTrue(rc, 'Expected ' + fn + ' to be appendeable');
  }

  function tryChmodAllowed(fn) {
    let rc = fs.chmod(fn, '0755');
    assertTrue(rc, 'Expected ' + fn + ' to be chmodable');
  }

  function tryExistsAllowed(fn, exists) {
    let rc = fs.exists(fn);
    assertEqual(rc, exists, 'Expected ' + fn + ' to be stat-eable');
    if (exists) {
      rc = fs.mtime(fn);
      assertTrue(rc > 0);
    }
  }

  function tryFileSizeAllowed(fn) {
    let rc = fs.size(fn);
    assertTrue(rc, 'Expected ' + fn + ' to be stat-eable');
  }

  function tryIsDirectoryAllowed(fn, isDirectory) {
    let rc = fs.isDirectory(fn);
    assertEqual(rc, isDirectory, 'Expected ' + fn + ' to be a ' + isDirectory ? "directory.":"file.");
  }

  function tryCreateDirectoryAllowed(fn) {
    let rc = fs.makeDirectory(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to become a directory.');
    tryIsDirectoryAllowed(fn, true);
  }

  function tryRemoveDirectoryAllowed(fn) {
    let rc = fs.removeDirectory(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to become a directory.');
    tryIsDirectoryAllowed(fn, false);
  }

  function tryCreateDirectoryRecursiveAllowed(fn) {
    let rc = fs.makeDirectoryRecursive(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to become a directory.');
    tryIsDirectoryAllowed(fn, true);
  }

  function tryRemoveDirectoryRecursiveAllowed(fn) {
    let rc = fs.removeDirectoryRecursive(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to be removed.');
    tryIsDirectoryAllowed(fn, false);
  }

  function tryIsFileAllowed(fn, isFile) {
    let rc = fs.isFile(fn);
    assertEqual(rc, isFile, 'Expected ' + fn + ' to be a ' + isFile ? "file.":"directory.");
  }

  function tryListFileAllowed(dn, expectCount) {
    let rc = fs.list(dn);
    assertEqual(rc.length, expectCount, 'Expected ' + dn + ' to contain ' + expectCount + " files.");
  }

  function tryListTreeAllowed(dn, expectCount) {
    let rc = fs.listTree(dn);
    assertEqual(rc.length, expectCount, 'Expected ' + dn + ' to contain ' + expectCount + " files.");
  }

  function tryGetTempFileAllowed(dn) {
    let tfn = fs.getTempFile();
    fs.write(tfn, "hello");
    assertTrue(fs.isFile(tfn));
    fs.remove(tfn);
  }

  function tryMakeAbsoluteAllowed(fn) {
    fs.makeAbsolute(fn);
  }
  function tryCopyFileAllowed(sn, tn) {
    fs.copyFile(sn, tn);
    tryExistsAllowed(sn, true);
    tryExistsAllowed(tn, true);
  }
  function tryCopyRecursiveFileAllowed(sn, tn) {
    fs.copyRecursive(sn, tn);
    tryExistsAllowed(sn, true);
    tryExistsAllowed(tn, true);
  }
  function tryMoveFileAllowed(sn, tn) {
    fs.move(sn, tn);
    tryExistsAllowed(sn, false);
    tryExistsAllowed(tn, true);
  }
  function tryRemoveFileAllowed(fn) {
    fs.remove(fn);
    tryExistsAllowed(fn, false);
  }
  function tryLinkFileAllowed(sn, tn) {
    fs.linkFile(sn, tn);
    tryExistsAllowed(sn, true);
    tryExistsAllowed(tn, true);
  }

  function tryZipFileAllowed(zip, sourceDir) {
    let files = [];
    try {
      files = fs.list(sourceDir).filter(function (fn) {
        if (fn === 'into_forbidden.txt') {
          return false;
        }
        return fs.isFile(fs.join(sourceDir, fn));
      });
      fs.zipFile(zip, sourceDir, files);
    } catch (err) {
      assertTrue(false, "failed to Zip into " + zip + " these files: " + sourceDir + "[ " + files + " ] - " + err);
    }
    tryExistsAllowed(zip, true);
  }

  function tryUnZipFileAllowed(zip, sn) {
    try {
      fs.unzipFile(zip, sn, false, true);
    } catch (err) {
      assertTrue(false, "failed to UnZip " + zip + " to " + sn + " - " + err);
    }
  }

  function tryJSParseFileAllowed(fn) {
    try {
      require("internal").parseFile(fn);
    } catch (err) {
      assertTrue(false, "was forbidden to parse: " + fn + " - " + err);
    }
  }

  function tryJSEvalFileAllowed(fn) {
    try {
      require("internal").load(fn);
    } catch (err) {
      assertTrue(false, "was forbidden to load: " + fn + " - " + err);
    }
  }

  const geil = 'geil';
  const ballern = 'ballern';
  const gb = geil + ballern;

  const CSV = 'der,reiher,hat,5,kleine,Eier\n';
  const CSVParsed = [['der','reiher','hat','5','kleine','Eier']];

  const JSONText = '{"a": true, "b":false, "c": "abc"}\n{"a": true, "b":false, "c": "abc"}';
  const JSONParsed = { "a" : true, "b" : false, "c" : "abc"};



  return {
    testSetupTempDir : function() {
      tryCreateDirectoryRecursiveAllowed(fs.join(topLevelAllowed, 'allowed_create_recursive_dir', 'directory'));
      tryCreateDirectoryRecursiveAllowed(topLevelAllowedRecursive);
      tryGetTempFileAllowed(topLevelAllowed);
    },

    testWriteReadAppendFile : function() {
      tryWriteAllowed(topLevelAllowedFile, geil);
      tryReadAllowed(topLevelAllowedFile, geil);
      tryAppendAllowed(topLevelAllowedFile, ballern);
      tryReadAllowed(topLevelAllowedFile, gb);
    },
    testReadFile : function() {
      tryAdler32Allowed(topLevelAllowedFile, gb);
    },
    testReadBuffer : function() {
      tryReadBufferAllowed(topLevelAllowedFile, gb);
    },
    testRead64File : function() {
      tryRead64Allowed(topLevelAllowedFile, gb);
    },
    testReadCSVFile : function() {
      tryWriteAllowed(topLevelAllowedReadCSVFile, CSV);
      tryReadCSVAllowed(topLevelAllowedReadCSVFile, CSV);
    },
    testReadJSONFile : function() {
      tryWriteAllowed(topLevelAllowedReadJSONFile, JSONText);
      tryReadJSONAllowed(topLevelAllowedReadJSONFile, JSONText);
    },
    testChmod : function() {
      tryChmodAllowed(topLevelAllowedFile);
    },
    testExists : function() {
      tryExistsAllowed(topLevelAllowedFile, true);
    },
    testFileSize : function() {
      tryFileSizeAllowed(topLevelAllowedFile);
    },
    testIsDirectory : function() {
      tryIsDirectoryAllowed(topLevelAllowedFile, false);
      tryIsDirectoryAllowed(topLevelAllowed, true);
    },
    testIsFile : function() {
      tryIsFileAllowed(topLevelAllowedFile, true);
      tryIsFileAllowed(topLevelAllowed, false);
    },
    testListFile : function() {
      tryListFileAllowed(topLevelAllowedFile, 0);
      tryListFileAllowed(topLevelAllowed, 4);
    },
    testListTree : function() {
      tryListTreeAllowed(topLevelAllowedFile, 1);
    },
    testZip : function() {

      assertTrue(fs.isDirectory(topLevelAllowed));
      assertFalse(fs.isFile(allowedZipFileName));
      tryZipFileAllowed(allowedZipFileName, topLevelAllowed);
      tryUnZipFileAllowed(allowedZipFileName, topLevelAllowedUnZip);
    },
    testCreateRemoveDirectory : function() {
      tryCreateDirectoryAllowed(fs.join(topLevelAllowed, "allowed_create_dir"));
      tryRemoveDirectoryAllowed(fs.join(topLevelAllowed, "allowed_create_dir"));
    },
    testCreateRemoveDirectoryRecursive : function() {
      tryCreateDirectoryRecursiveAllowed(fs.join(topLevelAllowed, 'allowed_create_recursive_dir', 'directory'));
    },
    testCopyRecursive : function() {
      tryCopyRecursiveFileAllowed(topLevelAllowed, topLevelAllowed + "wallah ich schwöre");
    },
  };
}
jsunity.run(testSuite);
return jsunity.done();
