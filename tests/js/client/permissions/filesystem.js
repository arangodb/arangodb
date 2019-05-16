/*jshint globalstrict:false, strict:false */
/* global fail, getOptions, assertTrue, assertEqual, assertNotEqual, assertUndefined */

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

//  first inst - tmp  --                 /tmp/xxx-arangosh/
//  first inst - rootDir  --             /tmp/xxx-arangosh/permissions
//  first inst - testFilesDir  --        /tmp/xxx-arangosh/permissions/testfiles
//  frist inst - subInstanceTemp  --     /tmp/xxx-arangosh/permissions/subinstance_temp_directory

// second inst - tmp  --                 /tmp/xxx-arangosh/permissions/subinstance_temp_directory
// second inst - rootDir  --             /tmp/xxx-arangosh/permissions
// second inst - testFilesDir  --        /tmp/xxx-arangosh/permissions/testfiles
// second inst - subInstanceTemp  --     not needed /tmp/xxx-arangosh/permissions/subinstance_tmp_directory


let rootDir = fs.join(fs.getTempPath(), '..');
let subInstanceTemp; //not set for subinstance
let testFilesDir = fs.join(rootDir, 'test_file_tree');

if (getOptions === true) {
  rootDir = fs.join(fs.getTempPath(), 'permissions');
  subInstanceTemp = fs.join(rootDir, 'subinstance_temp_directory');
  testFilesDir = fs.join(rootDir, 'test_file_tree');
  fs.makeDirectoryRecursive(subInstanceTemp);
  fs.makeDirectoryRecursive(testFilesDir);

}


const topLevelForbidden = fs.join(testFilesDir, 'forbidden');
const forbiddenZipFileName = fs.join(topLevelForbidden, 'forbidden.zip');
const forbiddenJSFileName = fs.join(topLevelForbidden, 'forbidden.js');
const topLevelForbiddenRecursive = fs.join(testFilesDir, 'forbidden_recursive');

const topLevelAllowed = fs.join(testFilesDir, 'allowed');
const intoTopLevelForbidden = fs.join(topLevelAllowed, 'into_forbidden.txt');
const topLevelAllowedUnZip = fs.join(testFilesDir, 'allowed_unzip');
const topLevelAllowedRecursive = fs.join(testFilesDir, 'allowed_recursive');
const allowedZipFileName = fs.join(topLevelAllowedRecursive, 'allowed.zip');
const allowedJSFileName = fs.join(topLevelAllowedRecursive, 'allowed.js');
const intoTopLevelAllowed = fs.join(topLevelForbidden, 'into_allowed.txt');

const topLevelAllowedFile = fs.join(topLevelAllowed, 'allowed.txt');
const topLevelForbiddenFile = fs.join(topLevelForbidden, 'forbidden.txt');

// N/A const subLevelForbidden = fs.join(topLevelAllowed, 'forbidden');
const subLevelAllowed = fs.join(topLevelForbidden, 'allowed');
const intoSubLevelAllowed = topLevelForbidden + '/allowed/aoeu';

const subLevelAllowedFile = fs.join(subLevelAllowed, 'allowed.txt');
// N/A const subLevelForbiddenFile = fs.join(subLevelForbidden, 'forbidden.txt');

const topLevelAllowedWriteFile = fs.join(topLevelAllowed, 'allowed_write.txt');
const topLevelForbiddenWriteFile = fs.join(topLevelForbidden, 'forbidden_write.txt');
const subLevelAllowedWriteFile = fs.join(subLevelAllowed, 'allowed_write.txt');
// N/A const subLevelForbiddenWriteFile = fs.join(subLevelForbidden, 'forbidden_write.txt');

const topLevelAllowedReadCSVFile = fs.join(topLevelAllowed, 'allowed_csv.txt');
const topLevelForbiddenReadCSVFile = fs.join(topLevelForbidden, 'forbidden_csv.txt');
const subLevelAllowedReadCSVFile = fs.join(subLevelAllowed, 'allowed_csv.txt');
// N/A const subLevelForbiddenReadCSVFile = fs.join(subLevelForbidden, 'forbidden_csv.txt');

const topLevelAllowedReadJSONFile = fs.join(topLevelAllowed, 'allowed_json.txt');
const topLevelForbiddenReadJSONFile = fs.join(topLevelForbidden, 'forbidden_json.txt');
const subLevelAllowedReadJSONFile = fs.join(subLevelAllowed, 'allowed_json.txt');
// N/A const subLevelForbiddenReadJSONFile = fs.join(subLevelForbidden, 'forbidden_json.txt');


const topLevelAllowedCopyFile = fs.join(topLevelAllowed, 'allowed_copy.txt');
const topLevelForbiddenCopyFile = fs.join(topLevelForbidden, 'forbidden_copy.txt');
const subLevelAllowedCopyFile = fs.join(subLevelAllowed, 'allowed_copy.txt');
// N/A const subLevelForbiddenCopyFile = fs.join(subLevelForbidden, 'forbidden_json.txt');

const CSV = 'a,b\n1,2\n3,4\n';
const CSVParsed = [['a', 'b'], ['1', '2'], ['3', '4']];
const JSONText = '{"a": true, "b":false, "c": "abc"}\n{"a": true, "b":false, "c": "abc"}';
const JSONParsed = { "a" : true, "b" : false, "c" : "abc"};

if (getOptions === true) {
  // N/A fs.makeDirectoryRecursive(subLevelForbidden);
  fs.makeDirectoryRecursive(topLevelAllowed);
  fs.makeDirectoryRecursive(subLevelAllowed);
  fs.makeDirectoryRecursive(topLevelAllowedRecursive);
  fs.makeDirectoryRecursive(topLevelForbiddenRecursive);
  fs.makeDirectoryRecursive(topLevelAllowedUnZip);
  fs.write(topLevelAllowedFile, 'this file is allowed.\n');
  fs.write(topLevelForbiddenFile, 'forbidden fruits are tasty!\n');
  fs.write(subLevelAllowedFile, 'this file is allowed.\n');
   // N/A fs.write(subLevelForbiddenFile, 'forbidden fruits are tasty!\n');

  fs.write(forbiddenJSFileName, `print('hello world');\n`);
  fs.write(allowedJSFileName, `print('hello world');\n`);


  fs.write(topLevelAllowedCopyFile, 'this file is allowed.\n');
  fs.write(topLevelForbiddenCopyFile, 'forbidden fruits are tasty!\n');
  fs.write(subLevelAllowedCopyFile, 'this file is allowed.\n');
   // N/A fs.write(subLevelForbiddenFile, 'forbidden fruits are tasty!\n');

  try {
    fs.linkFile(topLevelForbiddenFile, intoTopLevelForbidden);
    fs.linkFile(topLevelAllowedFile, intoTopLevelAllowed);
  } catch (ex) {
    internal.print("unable to create symlinks" + ex);
  }

  fs.write(topLevelAllowedReadCSVFile, CSV);
  fs.write(topLevelForbiddenReadCSVFile, CSV);
  fs.write(subLevelAllowedReadCSVFile, CSV);

  fs.write(topLevelAllowedReadJSONFile, JSONText);
  fs.write(topLevelForbiddenReadJSONFile, JSONText);
  fs.write(subLevelAllowedReadJSONFile, JSONText);

  return {
    'temp.path': subInstanceTemp,     // Adjust the temp-path to match our current temp path
    'javascript.files-whitelist': [
     fs.escapePath('^' + process.env['RESULT']),
     fs.escapePath('^' + topLevelAllowed),
     fs.escapePath('^' + subLevelAllowed),
     fs.escapePath('^' + topLevelAllowedRecursive)
    ]
  };
}

var jsunity = require('jsunity');
var env = require('process').env;
var arangodb = require("@arangodb");
const base64Encode = require('internal').base64Encode;
function testSuite() {
  function tryReadForbidden(fn) {
    try {
      fs.read(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryReadAllowed(fn, expectedContent) {
    let content = fs.read(fn);
    assertEqual(content,
                expectedContent,
                'Expected ' + fn + ' to contain "' + expectedContent + '" but it contained: "' + content + '"!');
  }
  function tryReadBufferForbidden(fn) {
    try {
      fs.readBuffer(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryReadBufferAllowed(fn, expectedContent) {
    let content = fs.readBuffer(fn);
    assertEqual(content.toString(),
                expectedContent,
                'Expected ' + fn + ' to contain "' + expectedContent + '" but it contained: "' + content + '"!');
  }
  function tryRead64Forbidden(fn) {
    try {
      fs.read64(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryRead64Allowed(fn, expectedContentPlain) {
    let expectedContent = base64Encode(expectedContentPlain);
    let content = fs.read64(fn);
    assertEqual(content,
                expectedContent,
                'Expected ' + fn + ' to contain "' + expectedContent + '" but it contained: "' + content + '"!');
  }
  function tryReadCSVForbidden(fn) {
    try {
      internal.processCsvFile(fn, function (raw_row, index) {
      });
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'access to ' + fn + ' wasn\'t forbidden');
    }
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
  function tryReadJSONForbidden(fn) {
    try {
      internal.processJsonFile(fn, function (raw_row, index) {
      });
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'access to ' + fn + ' wasn\'t forbidden');
    }
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
  function tryAdler32Forbidden(fn) {
    try {
      fs.adler32(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'access to ' + fn + ' wasn\'t forbidden: ' + err);
    }
  }
  function tryAdler32Allowed(fn, expectedContentPlain) {
    let content = fs.adler32(fn);
    assertTrue(content > 0);
  }
  function tryWriteForbidden(fn) {
    try {
      let rc = fs.write(fn, '1212 this is just a test');
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Write access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryWriteAllowed(fn) {
    let rc = fs.write(fn, '1212 this is just a test');
    assertTrue(rc, 'Expected ' + fn + ' to be writeable');
  }

  function tryAppendForbidden(fn) {
    try {
      let rc = fs.append(fn, '1212 this is just a test');
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Append access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryAppendAllowed(fn) {
    let rc = fs.append(fn, '1212 this is just a test');
    assertTrue(rc, 'Expected ' + fn + ' to be appendeable');
  }

  function tryChmodForbidden(fn) {
    try {
      let rc = fs.chmod(fn, '0755');
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'chmod access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryChmodAllowed(fn) {
    let rc = fs.chmod(fn, '0755');
    assertTrue(rc, 'Expected ' + fn + ' to be chmodable');
  }

  function tryExistsForbidden(fn) {
    try {
      let rc = fs.exists(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'stat-access to ' + fn + ' wasn\'t forbidden');
    }
    try {
      let rc = fs.mtime(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'stat-access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryExistsAllowed(fn, exists) {
    let rc = fs.exists(fn);
    assertEqual(rc, exists, 'Expected ' + fn + ' to be stat-eable');
    if (exists) {
      rc = fs.mtime(fn);
      assertTrue(rc > 0);
    }
  }

  function tryFileSizeForbidden(fn) {
    try {
      let rc = fs.size(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'stat-access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryFileSizeAllowed(fn) {
    let rc = fs.size(fn);
    assertTrue(rc, 'Expected ' + fn + ' to be stat-eable');
  }

  function tryIsDirectoryForbidden(fn) {
    try {
      let rc = fs.isDirectory(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'isDirectory-access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryIsDirectoryAllowed(fn, isDirectory) {
    let rc = fs.isDirectory(fn);
    assertEqual(rc, isDirectory, 'Expected ' + fn + ' to be a ' + isDirectory ? "directory.":"file.");
  }

  function tryCreateDirectoryForbidden(fn) {
    try {
      let rc = fs.makeDirectory(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'createDirectory creation of ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryCreateDirectoryAllowed(fn) {
    let rc = fs.makeDirectory(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to become a directory.');
    tryIsDirectoryAllowed(fn, true);
  }

  function tryRemoveDirectoryForbidden(fn) {
    try {
      let rc = fs.removeDirectory(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'removeDirectory deleting of ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryRemoveDirectoryAllowed(fn) {
    let rc = fs.removeDirectory(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to become a directory.');
    tryIsDirectoryAllowed(fn, false);
  }

  function tryCreateDirectoryRecursiveForbidden(fn) {
    try {
      let rc = fs.makeDirectoryRecursive(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'createDirectory creation of ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryCreateDirectoryRecursiveAllowed(fn) {
    let rc = fs.makeDirectoryRecursive(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to become a directory.');
    tryIsDirectoryAllowed(fn, true);
  }

  function tryRemoveDirectoryRecursiveForbidden(fn) {
    try {
      let rc = fs.removeDirectoryRecursive(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'removeDirectoryRecursive deletion of ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryRemoveDirectoryRecursiveAllowed(fn) {
    let rc = fs.removeDirectoryRecursive(fn);
    assertUndefined(rc, 'Expected ' + fn + ' to be removed.');
    tryIsDirectoryAllowed(fn, false);
  }

  function tryIsFileForbidden(fn) {
    try {
      let rc = fs.isFile(fn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'isFile-access to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryIsFileAllowed(fn, isFile) {
    let rc = fs.isFile(fn);
    assertEqual(rc, isFile, 'Expected ' + fn + ' to be a ' + isFile ? "file.":"directory.");
  }

  function tryListFileForbidden(dn) {
    try {
      let rc = fs.list(dn);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'ListFile-access to ' + dn + ' wasn\'t forbidden');
    }
  }
  function tryListFileAllowed(dn, expectCount) {
    let rc = fs.list(dn);
    assertEqual(rc.length, expectCount, 'Expected ' + dn + ' to contain ' + expectCount + " files.");
  }

  function tryListTreeForbidden(dn) {
    try {
      let rc = fs.listTree(dn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'ListTree-access to ' + dn + ' wasn\'t forbidden');
    }
  }
  function tryListTreeAllowed(dn, expectCount) {
    let rc = fs.listTree(dn);
    assertEqual(rc.length, expectCount, 'Expected ' + dn + ' to contain ' + expectCount + " files.");
  }

  function tryGetTempFileForbidden(dn) {
    try {
      let rc = fs.getTempFile(dn, true);
      fail();
    }
    catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Temfile-access to ' + dn + ' wasn\'t forbidden');
    }
  }
  function tryGetTempFileAllowed(dn) {
    let tfn = fs.getTempFile();
    assertTrue(fs.isFile(tfn));
    fs.remove(tfn);
  }

  function tryMakeAbsoluteForbidden(fn) {
    try {
      let absolute = fs.makeAbsolute(fn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Absolute expansion to ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryMakeAbsoluteAllowed(fn) {
    fs.makeAbsolute(fn);
  }
  function tryCopyFileForbidden(sn, tn) {
    try {
      let absolute = fs.copyFile(sn, tn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Copying of ' + sn + ' to ' + tn + ' wasn\'t forbidden');
    }
  }
  function tryCopyFileAllowed(sn, tn) {
    try {
      fs.copyFile(sn, tn);
    } catch (err) {
      assertTrue(false, "failed to copy " + sn + " to " + tn + " - " + err);
    }
    tryExistsAllowed(sn, true);
    tryExistsAllowed(tn, true);
  }
  function tryCopyRecursiveFileForbidden(sn, tn) {
    try {
      let absolute = fs.copyRecursive(sn, tn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'CopyRecursiveing of ' + sn + ' to ' + tn + ' wasn\'t forbidden: ' + err);
    }
  }
  function tryCopyRecursiveFileAllowed(sn, tn) {
    try {
      fs.copyRecursive(sn, tn);
    } catch (err) {
      assertTrue(false, "failed to copy " + sn + " to " + tn + " - " + err);
    }
    tryExistsAllowed(sn, true);
    tryExistsAllowed(tn, true);
  }
  function tryMoveFileForbidden(sn, tn) {
    try {
      let rc = fs.move(sn, tn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Moving of ' + sn + ' to ' + tn + ' wasn\'t forbidden');
    }
  }
  function tryMoveFileAllowed(sn, tn) {
    try {
      fs.move(sn, tn);
    } catch (err) {
      assertTrue(false, "failed to move " + sn + " to " + tn + " - " + err);
    }
    tryExistsAllowed(sn, false);
    tryExistsAllowed(tn, true);
  }
  function tryRemoveFileForbidden(fn) {
    try {
      let rc = fs.remove(fn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'removing of ' + fn + ' wasn\'t forbidden');
    }
  }
  function tryRemoveFileAllowed(fn) {
    try {
      fs.remove(fn);
    } catch (err) {
      fail("failed to remove " + fn + " - " + err.Message);
    }
    tryExistsAllowed(fn, false);
  }
  function tryLinkFileForbidden(tn, sn) {
    try {
      let rc = fs.linkFile(tn, sn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Moving of ' + sn + ' to ' + tn + ' wasn\'t forbidden');
    }
  }
  function tryLinkFileAllowed(sn, tn) {
    try {
      fs.linkFile(sn, tn);
    } catch (err) {
      assertTrue(false, "failed to Link " + sn + " into " + tn + " - " + err);
    }
    tryExistsAllowed(sn, true);
    tryExistsAllowed(tn, true);
  }

  function tryZipFileForbidden(zip, sn) {
    try {
      let rc = fs.zipFile(zip, sn, ['*']);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Zipping of ' + zip + ' to ' + sn + ' wasn\'t forbidden: ' + err);
    }
  }
  function tryZipFileAllowed(zip, sn) {
    let files = [];
    try {
      files = fs.list(sn).filter(function (fn) {
        if (fn === 'into_forbidden.txt') {
          return false;
        }
        return fs.isFile(fs.join(sn, fn));
      });
      fs.zipFile(zip, sn, files);
    } catch (err) {
      assertTrue(false, "failed to Zip into " + zip + " these files:" + sn + "[ " + files + " ] - " + err);
    }
    tryExistsAllowed(zip, true);
  }

  function tryUnZipFileForbidden(zip, sn) {
    try {
      let rc = fs.unzipFile(zip, sn, undefined, true);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'Unzipping of ' + zip + ' to ' + sn + ' wasn\'t forbidden: ' + err);
    }
  }
  function tryUnZipFileAllowed(zip, sn) {
    try {
      fs.unzipFile(zip, sn, false, true);
    } catch (err) {
      assertTrue(false, "failed to UnZip " + zip + " to " + sn + " - " + err);
    }
  }

  function tryJSParseFileForbidden(fn) {
    try {
      require("internal").parseFile(fn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, "wasn't forbidden to parse: " + fn);
    }
  }
  function tryJSParseFileAllowed(fn) {
    try {
      require("internal").parseFile(fn);
    } catch (err) {
      assertTrue(false, "was forbidden to parse: " + fn + " - " + err);
    }
  }

  function tryJSEvalFileForbidden(fn) {
    try {
      require("internal").load(fn);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, "wasn't forbidden to load: " + fn);
    }
  }
  function tryJSEvalFileAllowed(fn) {
    try {
      require("internal").load(fn);
    } catch (err) {
      assertTrue(false, "was forbidden to load: " + fn + " - " + err);
    }
  }

  return {
    testGetTempFile : function() {
      //tryGetTempFileForbidden('/etc/');     //NONSENSE == /tmp/arango-xxxx/etc/ is totally fine
      //tryGetTempFileForbidden('/var/log/');
      //tryGetTempFileForbidden(topLevelForbidden);
      //FIXMEtryGetTempFileAllowed(topLevelAllowed);
      //FIXME//tryGetTempFileAllowed(subLevelAllowed);
    },
    testReadFile : function() {
      tryReadForbidden('/etc/passwd');
      tryReadForbidden('/var/log/mail.log');
      tryReadForbidden(topLevelForbiddenFile);
      // N/A tryReadForbidden(subLevelForbiddenFile);

      tryReadAllowed(topLevelAllowedFile, 'this file is allowed.\n');
      tryReadAllowed(subLevelAllowedFile, 'this file is allowed.\n');

      tryAdler32Forbidden('/etc/passwd');
      tryAdler32Forbidden('/var/log/mail.log');
      tryAdler32Forbidden(topLevelForbiddenFile);
      // N/A tryAdler32Forbidden(subLevelForbiddenFile);

      tryAdler32Allowed(topLevelAllowedFile, 'this file is allowed.\n');
      tryAdler32Allowed(subLevelAllowedFile, 'this file is allowed.\n');
    },
    testReadBuffer : function() {
      tryReadBufferForbidden('/etc/passwd');
      tryReadBufferForbidden('/var/log/mail.log');
      tryReadBufferForbidden(topLevelForbiddenFile);
      // N/A tryReadForbidden(subLevelForbiddenFile);

      tryReadBufferAllowed(topLevelAllowedFile, 'this file is allowed.\n');
      tryReadBufferAllowed(subLevelAllowedFile, 'this file is allowed.\n');
    },
    testRead64File : function() {

      tryRead64Forbidden('/etc/passwd');
      tryRead64Forbidden('/var/log/mail.log');
      tryRead64Forbidden(topLevelForbiddenFile);
      // N/A tryReadForbidden(subLevelForbiddenFile);

      tryRead64Allowed(topLevelAllowedFile, 'this file is allowed.\n');
      tryRead64Allowed(subLevelAllowedFile, 'this file is allowed.\n');
    },
    testReadCSVFile : function() {
      tryReadCSVForbidden('/etc/passwd');
      tryReadCSVForbidden('/var/log/mail.log');
      tryReadCSVForbidden(topLevelForbiddenReadCSVFile);
      // N/A tryReadCSVForbidden(subLevelForbiddenReadCSVFile);

      tryReadCSVAllowed(topLevelAllowedReadCSVFile, CSV);
      tryReadCSVAllowed(subLevelAllowedReadCSVFile, CSV);
    },
    testReadJSONFile : function() {
      tryReadJSONForbidden('/etc/passwd');
      tryReadJSONForbidden('/var/log/mail.log');
      tryReadJSONForbidden(topLevelForbiddenReadJSONFile);
      // N/A tryReadJSONForbidden(subLevelForbiddenReadJSONFile);

      tryReadJSONAllowed(topLevelAllowedReadJSONFile, JSONText);
      tryReadJSONAllowed(subLevelAllowedReadJSONFile, JSONText);
    },
    testWriteFile : function() {
      tryWriteForbidden('/var/log/mail.log');
      tryWriteForbidden(topLevelForbiddenWriteFile);
      tryAppendForbidden('/var/log/mail.log');
      tryAppendForbidden(topLevelForbiddenWriteFile);

      tryWriteAllowed(topLevelAllowedWriteFile);
      tryWriteAllowed(subLevelAllowedWriteFile);
      tryAppendAllowed(topLevelAllowedWriteFile);
      tryAppendAllowed(subLevelAllowedWriteFile);

    },
    testChmod : function() {
      tryChmodForbidden('/etc/passwd');
      tryChmodForbidden('/var/log/mail.log');
      tryChmodForbidden(topLevelForbiddenFile);
      // N/A tryChmodForbidden(subLevelForbiddenFile);

      tryChmodAllowed(topLevelAllowedFile);
      tryChmodAllowed(subLevelAllowedFile);
    },
    testExists : function() {
      tryExistsForbidden('/etc/passwd');
      tryExistsForbidden('/var/log/mail.log');
      tryExistsForbidden(topLevelForbiddenFile);
      // N/A tryExistsForbidden(subLevelForbiddenFile);

      tryExistsAllowed(topLevelAllowedFile, true);
      tryExistsAllowed(subLevelAllowedFile, true);
    },
    testFileSize : function() {
      tryFileSizeForbidden('/etc/passwd');
      tryFileSizeForbidden('/var/log/mail.log');
      tryFileSizeForbidden(topLevelForbiddenFile);
      // N/A tryFileSizeForbidden(subLevelForbiddenFile);

      tryFileSizeAllowed(topLevelAllowedFile);
      tryFileSizeAllowed(subLevelAllowedFile);
    },
    testIsDirectory : function() {
      tryIsDirectoryForbidden('/etc/passwd');
      tryIsDirectoryForbidden('/var/log/mail.log');
      tryIsDirectoryForbidden(topLevelForbiddenFile);
      // N/A tryFileSizeForbidden(subLevelForbiddenFile);

      tryIsDirectoryAllowed(topLevelAllowedFile, false);
      tryIsDirectoryAllowed(subLevelAllowedFile, false);

      tryIsDirectoryAllowed(topLevelAllowed, true);
      tryIsDirectoryAllowed(subLevelAllowed, true);
    },
    testCreateRemoveDirectory : function() {
      tryCreateDirectoryForbidden('/etc/abc');
      tryCreateDirectoryForbidden('/var/log/busted');
      tryCreateDirectoryForbidden(fs.join(topLevelForbiddenFile, "forbidden"));

      tryCreateDirectoryAllowed(fs.join(topLevelAllowed, "allowed_create_dir"));
      tryCreateDirectoryAllowed(fs.join(subLevelAllowed, "allowed_create_dir"));

      tryRemoveDirectoryForbidden('/etc/abc');
      tryRemoveDirectoryForbidden('/var/log/busted');
      tryRemoveDirectoryForbidden(fs.join(topLevelForbiddenFile, "forbidden"));

      tryRemoveDirectoryAllowed(fs.join(topLevelAllowed, "allowed_create_dir"));
      tryRemoveDirectoryAllowed(fs.join(subLevelAllowed, "allowed_create_dir"));
    },
    testCreateRemoveDirectoryRecursive : function() {
      tryCreateDirectoryRecursiveForbidden('/etc/cde/fdg');
      tryCreateDirectoryRecursiveForbidden('/var/log/with/sub/dir');
      tryCreateDirectoryRecursiveForbidden(fs.join(topLevelForbiddenFile, "forbidden", 'sub', 'directory'));

      tryCreateDirectoryRecursiveAllowed(fs.join(topLevelAllowed, 'allowed_create_recursive_dir', 'directory'));
      tryCreateDirectoryRecursiveAllowed(fs.join(subLevelAllowed, 'allowed_create_recursive_dir', 'directory'));

      tryRemoveDirectoryRecursiveForbidden('/etc/cde/fdg');
      tryRemoveDirectoryRecursiveForbidden('/var/log/with/sub/dir');
      tryRemoveDirectoryRecursiveForbidden(fs.join(topLevelForbiddenFile, "forbidden", 'sub', 'directory'));

      //tryRemoveDirectoryRecursiveAllowed(fs.join(topLevelAllowed, 'allowed_create_recursive_dir', 'directory')); //outside temp dir
      //tryRemoveDirectoryRecursiveAllowed(fs.join(subLevelAllowed, 'allowed_create_recursive_dir', 'directory'));
    },
    testIsFile : function() {
      tryIsFileForbidden('/etc/passwd');
      tryIsFileForbidden('/var/log/mail.log');
      tryIsFileForbidden(topLevelForbiddenFile);
      // N/A tryFileSizeForbidden(subLevelForbiddenFile);

      tryIsFileAllowed(topLevelAllowedFile, true);
      tryIsFileAllowed(subLevelAllowedFile, true);

      tryIsFileAllowed(topLevelAllowed, false);
      tryIsFileAllowed(subLevelAllowed, false);
    },
    testListFile : function() {
      tryListFileForbidden('/etc/X11');
      tryListFileForbidden('/var/log/');
      tryListFileForbidden(topLevelForbidden);
      tryListFileForbidden(topLevelForbiddenFile);
      // N/A tryFileSizeForbidden(subLevelForbiddenFile);

      tryListFileAllowed(topLevelAllowedFile, 0);
      tryListFileAllowed(subLevelAllowedFile, 0);

      tryListFileAllowed(topLevelAllowed, 7);
      tryListFileAllowed(subLevelAllowed, 6);
    },
    testListTree : function() {
      tryListTreeForbidden('/etc/X11');
      tryListTreeForbidden('/var/log/');
      tryListTreeForbidden(topLevelForbidden);
      tryListTreeForbidden(topLevelForbiddenFile);
      // N/A tryFileSizeForbidden(subLevelForbiddenFile);

      tryListTreeAllowed(topLevelAllowedFile, 1);
      tryListTreeAllowed(subLevelAllowedFile, 1);

     //FIXME tryListTreeAllowed(topLevelAllowed, 8);
      //FIXMEtryListTreeAllowed(subLevelAllowed, 7);
    },
    testCopyReMoveLinkFiles : function() {
      tryCopyFileForbidden('/etc/passwd', topLevelAllowed);
      tryCopyFileForbidden(topLevelAllowedWriteFile, topLevelForbidden);
      tryLinkFileForbidden('/etc/passwd', topLevelAllowed);
      tryLinkFileForbidden(topLevelAllowedWriteFile, topLevelForbidden);
      tryMoveFileForbidden('/etc/passwd', topLevelAllowed);
      tryMoveFileForbidden(topLevelAllowedWriteFile, topLevelForbidden);
      tryRemoveFileForbidden('/etc/passwd');
      tryRemoveFileForbidden(topLevelForbiddenWriteFile);

      tryCopyRecursiveFileForbidden('/etc/passwd', fs.join(topLevelAllowedRecursive, 'sub', 'directory'));
      tryCopyRecursiveFileForbidden(subLevelAllowed, fs.join(topLevelForbiddenRecursive, 'sub', 'directory'));

      tryCopyFileAllowed(topLevelAllowedCopyFile, fs.join(topLevelAllowed, 'copy_1.txt'));
      tryCopyFileAllowed(subLevelAllowedCopyFile, fs.join(topLevelAllowed, 'copy_2.txt'));
      tryLinkFileAllowed(topLevelAllowedCopyFile, fs.join(topLevelAllowed, 'link_1.txt'));
      tryLinkFileAllowed(subLevelAllowedCopyFile, fs.join(topLevelAllowed, 'link_2.txt'));
      tryMoveFileAllowed(topLevelAllowedCopyFile, fs.join(topLevelAllowed, 'move_1.txt'));
      tryMoveFileAllowed(subLevelAllowedCopyFile, fs.join(topLevelAllowed, 'move_2.txt'));
      tryRemoveFileAllowed(fs.join(topLevelAllowed, 'move_1.txt'));
      tryRemoveFileAllowed(fs.join(topLevelAllowed, 'move_2.txt'));
      tryCopyRecursiveFileAllowed(subLevelAllowed, fs.join(topLevelAllowedRecursive, 'sub', 'directory'));
    },
    testZip : function() {
      tryZipFileForbidden(forbiddenZipFileName, topLevelAllowed);
      tryZipFileForbidden(allowedZipFileName, '/etc/');
      tryZipFileForbidden(allowedZipFileName, topLevelForbidden);

      tryZipFileAllowed(allowedZipFileName, topLevelAllowed);

      tryUnZipFileForbidden('/etc/nothere.zip', topLevelAllowed);
      tryUnZipFileForbidden(allowedZipFileName, topLevelForbidden);

      tryUnZipFileAllowed(allowedZipFileName, topLevelAllowedUnZip);
    },
    testEval : function() {
      tryJSParseFileForbidden(forbiddenJSFileName);
      tryJSParseFileAllowed(allowedJSFileName);
      tryJSEvalFileForbidden(forbiddenJSFileName);
      tryJSEvalFileAllowed(allowedJSFileName);

      // we can not forbid snippet evaluation.
      // Access is file access based for now.
      require("internal").parse(`print('hello world')`);
    }


  };
}
jsunity.run(testSuite);
return jsunity.done();
