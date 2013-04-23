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
      // create a name for a temporary directory we will run all tests in
      // as we are creating a new name, this shouldn't collide with any existing
      // directories, and we can also remove our directory when the tests are
      // over
      tempDir = fs.join(fs.getTempFile('', false));

      try {
        // create this directory before any tests
        fs.makeDirectoryRecursive(tempDir);
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // some sanity check as we don't want to unintentionally remove "." or "/"
      if (tempDir.length > 5) {
        // remove our temporary directory with all its subdirectories
        // we created it, so we don't care what's in it
        fs.removeDirectoryRecursive(tempDir);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief exists() - test if a file / directory exists
////////////////////////////////////////////////////////////////////////////////

    testExists : function () {
      var tempName;
      
      // create the name for a new directory
      tempName = fs.join(tempDir, 'foo');
      try {
        // remove it if it exists
        fs.removeDirectory(tempName);
      }
      catch (err) {
      }

      // create the directory with all paths to it
      fs.makeDirectoryRecursive(tempName);

      // now the directory should exist
      assertTrue(fs.exists(tempName));
     
      // remove it again
      fs.removeDirectory(tempName);

      // non-existing directory/file
      tempName = fs.join(tempDir, 'meow');
      // this file should not exist
      assertFalse(fs.exists(tempName));

      // create a new file
      tempName = fs.getTempFile('', true);
      // the file should now exist
      assertTrue(fs.exists(tempName));
      // clean it up
      fs.remove(tempName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief getTempFile()
////////////////////////////////////////////////////////////////////////////////

    testGetTempFile : function () {
      var tempName;

      // creating a new empty file with a temporary name
      tempName = fs.getTempFile('', true);

      assertTrue(tempName !== '');
      // the file should be located inside the tempPath
      assertEqual(fs.getTempPath(), tempName.substr(0, fs.getTempPath().length));
      // the file should exist
      assertTrue(fs.exists(tempName));

      // clean up
      fs.remove(tempName);
     
      // create a filename only, without creating the file itself
      tempName = fs.getTempFile('', false);
      assertTrue(tempName !== '');
      // filename should be located underneath tempPath
      assertEqual(fs.getTempPath(), tempName.substr(0, fs.getTempPath().length));
      // the file should not exist
      assertFalse(fs.exists(tempName));
      
      // create a temporary filename for a file in a subdirectory, without creating the file
      tempName = fs.getTempFile('tests', false);
      assertTrue(tempName !== '');
      // filename should be located underneath tempPath
      assertEqual(fs.join(fs.getTempPath(), 'test'), tempName.substr(0, fs.join(fs.getTempPath(), 'test').length));
      // file should not yet exist
      assertFalse(fs.exists(tempName));

      // clean up
      fs.removeDirectory(fs.join(fs.getTempPath(), 'tests'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief getTempPath() - test the system's temp path
////////////////////////////////////////////////////////////////////////////////

    testGetTempPath : function () {
      var tempName;

      tempName = fs.getTempPath();

      assertTrue(tempName !== '');
      // the temp path should also be an existing directory
      assertTrue(fs.isDirectory(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief isDirectory() - test if a directory exists
////////////////////////////////////////////////////////////////////////////////

    testIsDirectory : function () {
      var tempName;
      
      // create a new directory name
      tempName = fs.join(tempDir, 'foo');
      try {
        // remove any existing directory first
        fs.removeDirectory(tempName);
      }
      catch (err) {
      }

      // create the directory with the full path to it
      fs.makeDirectoryRecursive(tempName);

      // check if it does exist
      assertTrue(fs.exists(tempName));
      // should also be a directory
      assertTrue(fs.isDirectory(tempName));
      // remove the directory
      fs.removeDirectory(tempName);

      // non-existing directory/file
      tempName = fs.join(tempDir, 'meow');
      // this file shouldn't exist
      assertFalse(fs.exists(tempName));
      // and thus shouldn't be a directory
      assertFalse(fs.isDirectory(tempName));

      // create a new file
      tempName = fs.getTempFile('', true);
      // should exist
      assertTrue(fs.exists(tempName));
      // but shouldn't be a directory
      assertFalse(fs.isDirectory(tempName));

      // clean up
      fs.remove(tempName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief isFile() - test if a file exists
////////////////////////////////////////////////////////////////////////////////

    testIsFile : function () {
      var tempName;
      
      // create a new directory name
      tempName = fs.join(tempDir, 'foo');
      try {
        // remove any existing directory if it exists
        fs.removeDirectory(tempName);
      }
      catch (err) {
      }

      try {
        // remove any existing file if it exists
        fs.remove(tempName);
      }
      catch (err) {
      }

      // now create the whole directory with all paths to it
      fs.makeDirectoryRecursive(tempName);

      // directory should now exist
      assertTrue(fs.exists(tempName));
      // but should not be a file
      assertFalse(fs.isFile(tempName));
      // remove it
      fs.removeDirectory(tempName);
      
      // non-existing directory/file
      tempName = fs.join(tempDir, 'meow');
      // this file shouldn't exist
      assertFalse(fs.exists(tempName));
      // and shouldn't be a file
      assertFalse(fs.isFile(tempName));

      // now create a new file
      tempName = fs.getTempFile('', true);
      // should exist
      assertTrue(fs.exists(tempName));
      // and should be a file
      assertTrue(fs.isFile(tempName));

      // clean up and remove the file
      fs.remove(tempName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief listTree()
////////////////////////////////////////////////////////////////////////////////

    testListTree : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief makeDirectory() - create a directory
////////////////////////////////////////////////////////////////////////////////

    testMakeDirectory : function () {
      var tempName;
      
      // create the name for a subdirectory
      tempName = fs.join(tempDir, 'bar');
      try {
        // remove an existing directory if it already exists
        fs.removeDirectoryRecursive(tempName);
      }
      catch (err) {
      }

      // the directory shouldn't exist. we just deleted it if it was there
      assertFalse(fs.exists(tempName));

      // now create the directory
      fs.makeDirectory(tempName);

      // directory should be there after creation
      assertTrue(fs.exists(tempName));

      // now remove it
      fs.removeDirectory(tempName);
      
      // create the name for another subdirectory
      tempName = fs.join(tempDir, 'baz');
      try {
        // remove it if it does exist
        fs.removeDirectoryRecursive(tempName);
      }
      catch (err) {
      }

      // the directory shouldn't be there...
      assertFalse(fs.exists(tempName));

      // now create it
      fs.makeDirectory(tempName);

      // should have succeeded
      assertTrue(fs.exists(tempName));

      try {
        // try to create the same directory again. this should fail
        fs.makeDirectory(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_SYS_ERROR.code, err.errorNum);
      }

      // create a subdirectory in the directory we created
      tempName = fs.join(tempDir, 'baz', 'foo');
      fs.makeDirectory(tempName);
      // the subdirectory should now be there
      assertTrue(fs.exists(tempName));

      // create a file in the subdirecory
      tempName = fs.join(tempDir, 'baz', 'foo', 'test');
      // write something to the file
      fs.write(tempName, "this is a test");  
      // the file should exist after writing to it
      assertTrue(fs.exists(tempName));

      try {
        // create a directory with the name of an already existing file. this should fail.
        fs.makeDirectory(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_SYS_ERROR.code, err.errorNum);
      }

      // remove all stuff we created for testing
      fs.removeDirectoryRecursive(fs.join(tempDir, 'baz'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief makeDirectoryRecursive() - create a directory will all paths to it
////////////////////////////////////////////////////////////////////////////////

    testMakeDirectoryRecursive : function () {
      var tempName;
      
      // create the name for a new subdirectory
      tempName = fs.join(tempDir, 'bar');
      try {
        // make sure it does not yet exist
        fs.removeDirectoryRecursive(tempName);
      }
      catch (err) {
      }

      // create the subdirectory
      fs.makeDirectoryRecursive(tempName);
      // check if it is there
      assertTrue(fs.isDirectory(tempName));

      // create the subdirectory again. this should not fail
      fs.makeDirectoryRecursive(tempName);
      // should still be a directory
      assertTrue(fs.isDirectory(tempName));

      // create subdirectory in subdirectory of subdirectory
      tempName = fs.join(tempDir, 'bar', 'baz', 'test');
      fs.makeDirectoryRecursive(tempName);
      assertTrue(fs.isDirectory(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief move() - TODO
////////////////////////////////////////////////////////////////////////////////

    testMove : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read() - TODO
////////////////////////////////////////////////////////////////////////////////

    testRead : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove() - TODO
////////////////////////////////////////////////////////////////////////////////

    testRemove : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test size() - the filesize of a file
////////////////////////////////////////////////////////////////////////////////

    testSize : function () {
      var tempName;
      
      // create a new file with a specific content
      tempName = fs.join(tempDir, 'foo');
      fs.write(tempName, "this is a test file");

      // test the size of the new file
      assertEqual(19, fs.size(tempName));

      // remove the new file
      fs.remove(tempName);
    
      // now the file does not exist
      try { 
        // try to read filesize. this should fail
        fs.size(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }

      // directory
      fs.makeDirectory(tempName);
      try { 
        // try to read the filesize of a directory. this should fail
        fs.size(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }

      // remove the directory
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
