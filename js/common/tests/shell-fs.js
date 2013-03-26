////////////////////////////////////////////////////////////////////////////////
/// @brief test filesystem functions
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

var fs = require("fs");
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                        filesystem
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function FileSystemSuite () {
  var ERRORS = require("internal").errors;
  var tempDir;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      tempDir = fs.join(fs.getTempFile('', false));

      try {
        fs.makeDirectoryRecursive(tempDir);
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      if (tempDir.length > 5) {
        fs.removeDirectoryRecursive(tempDir);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief exists()
////////////////////////////////////////////////////////////////////////////////

    testExists : function () {
      var tempName;
      
      // existing directory
      tempName = fs.join(tempDir, 'foo');
      try {
        fs.removeDirectory(tempName);
      }
      catch (err) {
      }

      fs.makeDirectoryRecursive(tempName);

      assertTrue(fs.exists(tempName));
      fs.removeDirectory(tempName);

      // non-existing directory/file
      tempName = fs.join(tempDir, 'meow');
      assertFalse(fs.exists(tempName));

      // file
      tempName = fs.getTempFile('', true);
      assertTrue(fs.exists(tempName));
      fs.remove(tempName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief getTempFile()
////////////////////////////////////////////////////////////////////////////////

    testGetTempFile : function () {
      var tempName;

      // creating a new file
      tempName = fs.getTempFile('', true);

      assertTrue(tempName !== '');
      assertEqual(fs.getTempPath(), tempName.substr(0, fs.getTempPath().length));
      assertTrue(fs.exists(tempName));
      fs.remove(tempName);
     
      // without creating 
      tempName = fs.getTempFile('', false);
      assertTrue(tempName !== '');
      assertEqual(fs.getTempPath(), tempName.substr(0, fs.getTempPath().length));
      assertFalse(fs.exists(tempName));
      
      // in a subdirectory
      tempName = fs.getTempFile('tests', false);
      assertTrue(tempName !== '');
      assertEqual(fs.join(fs.getTempPath(), 'test'), tempName.substr(0, fs.join(fs.getTempPath(), 'test').length));
      assertFalse(fs.exists(tempName));
      fs.removeDirectory(fs.join(fs.getTempPath(), 'tests'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief getTempPath()
////////////////////////////////////////////////////////////////////////////////

    testGetTempPath : function () {
      var tempName;

      tempName = fs.getTempPath();

      assertTrue(tempName !== '');
      assertTrue(fs.isDirectory(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief isDirectory()
////////////////////////////////////////////////////////////////////////////////

    testIsDirectory : function () {
      var tempName;
      
      // existing directory
      tempName = fs.join(tempDir, 'foo');
      try {
        fs.removeDirectory(tempName);
      }
      catch (err) {
      }

      fs.makeDirectoryRecursive(tempName);

      assertTrue(fs.exists(tempName));
      assertTrue(fs.isDirectory(tempName));
      fs.removeDirectory(tempName);

      // non-existing directory/file
      tempName = fs.join(tempDir, 'meow');
      assertFalse(fs.exists(tempName));
      assertFalse(fs.isDirectory(tempName));

      // file
      tempName = fs.getTempFile('', true);
      assertTrue(fs.exists(tempName));
      assertFalse(fs.isDirectory(tempName));
      fs.remove(tempName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief isFile()
////////////////////////////////////////////////////////////////////////////////

    testIsFile : function () {
      var tempName;
      
      // existing directory
      tempName = fs.join(tempDir, 'foo');
      try {
        fs.removeDirectory(tempName);
      }
      catch (err) {
      }
      try {
        fs.remove(tempName);
      }
      catch (err) {
      }

      fs.makeDirectoryRecursive(tempName);
      assertTrue(fs.exists(tempName));
      assertFalse(fs.isFile(tempName));
      fs.removeDirectory(tempName);
      
      // non-existing directory/file
      tempName = fs.join(tempDir, 'meow');
      assertFalse(fs.exists(tempName));
      assertFalse(fs.isFile(tempName));

      // file
      tempName = fs.getTempFile('', true);
      assertTrue(fs.exists(tempName));
      assertTrue(fs.isFile(tempName));
      fs.remove(tempName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief listTree()
////////////////////////////////////////////////////////////////////////////////

    testListTree : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief makeDirectory()
////////////////////////////////////////////////////////////////////////////////

    testMakeDirectory : function () {
      var tempName;
      
      tempName = fs.join(tempDir, 'bar');
      try {
        fs.removeDirectoryRecursive(tempName);
      }
      catch (err) {
      }

      assertFalse(fs.exists(tempName));
      fs.makeDirectory(tempName);
      assertTrue(fs.exists(tempName));
      fs.removeDirectory(tempName);
      
      tempName = fs.join(tempDir, 'baz');
      try {
        fs.removeDirectoryRecursive(tempName);
      }
      catch (err) {
      }

      // make with an existing directory
      assertFalse(fs.exists(tempName));
      fs.makeDirectory(tempName);
      assertTrue(fs.exists(tempName));

      try {
        fs.makeDirectory(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_SYS_ERROR.code, err.errorNum);
      }

      // make subdirectory
      tempName = fs.join(tempDir, 'baz', 'foo');
      fs.makeDirectory(tempName);
      assertTrue(fs.exists(tempName));

      tempName = fs.join(tempDir, 'baz', 'foo', 'test');
      internal.write(tempName, "this is a test");  
      assertTrue(fs.exists(tempName));

      try {
        fs.makeDirectory(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_SYS_ERROR.code, err.errorNum);
      }

      fs.removeDirectoryRecursive(fs.join(tempDir, 'baz'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief makeDirectoryRecursive()
////////////////////////////////////////////////////////////////////////////////

    testMakeDirectoryRecursive : function () {
      var tempName;
      
      tempName = fs.join(tempDir, 'bar');
      try {
        fs.removeDirectoryRecursive(tempName);
      }
      catch (err) {
      }

      // create
      fs.makeDirectoryRecursive(tempName);
      assertTrue(fs.isDirectory(tempName));

      // create again
      fs.makeDirectoryRecursive(tempName);
      assertTrue(fs.isDirectory(tempName));

      // create subdirectories
      tempName = fs.join(tempDir, 'bar', 'baz', 'test');
      fs.makeDirectoryRecursive(tempName);
      assertTrue(fs.isDirectory(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief move()
////////////////////////////////////////////////////////////////////////////////

    testMove : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read()
////////////////////////////////////////////////////////////////////////////////

    testRead : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove()
////////////////////////////////////////////////////////////////////////////////

    testRemove : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief size()
////////////////////////////////////////////////////////////////////////////////

    testSize : function () {
      var tempName;
      
      // existing file
      tempName = fs.join(tempDir, 'foo');
      internal.write(tempName, "this is a test file");
      assertEqual(19, fs.size(tempName));
      fs.remove(tempName);
    
      // non-existing file
      try { 
        fs.size(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }

      // directory
      fs.makeDirectory(tempName);
      try { 
        fs.size(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }
      fs.removeDirectory(tempName);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(FileSystemSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
