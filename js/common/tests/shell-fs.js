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

// -----------------------------------------------------------------------------
// --SECTION--                                                        filesystem
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function FileSystemSuite () {
  var ERRORS = require("internal").errors;
  var tempDir;

  var binary = "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";

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
      // we need to call join() for the right-hand part too to normalise driver letters on Windows
      assertEqual(fs.join(fs.getTempPath(), 'test'), fs.join(tempName.substr(0, fs.join(fs.getTempPath(), 'test').length)));
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
        assertEqual(ERRORS.ERROR_FILE_EXISTS.code, err.errorNum);
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
        assertEqual(ERRORS.ERROR_FILE_EXISTS.code, err.errorNum);
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
/// @brief move() 
////////////////////////////////////////////////////////////////////////////////

    testMove : function () {
      var tempName, tempName2;
      
      // create a new file with a specific content
      tempName = fs.join(tempDir, 'foo');
      tempName2 = fs.join(tempDir, 'bar');
      fs.write(tempName, "this is a test file");

      fs.move(tempName, tempName2);
      assertFalse(fs.isFile(tempName));
      assertTrue(fs.isFile(tempName2));
      assertEqual("this is a test file", fs.read(tempName2));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief move() 
////////////////////////////////////////////////////////////////////////////////

    testMoveDirectory : function () {
      var tempName, tempName2;
      
      // create a new file with a specific content
      tempName = fs.join(tempDir, 'foo');
      tempName2 = fs.join(tempDir, 'bar');

      fs.makeDirectory(tempName);
      fs.makeDirectory(tempName2);

      fs.write(fs.join(tempName, "test"), "this is a test file");

      fs.move(tempName, tempName2);
      assertFalse(fs.isDirectory(tempName));
      assertTrue(fs.isDirectory(tempName2));
      assertTrue(fs.isFile(fs.join(tempName2, "test")));
      assertEqual("this is a test file", fs.read(fs.join(tempName2, "test")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read() 
////////////////////////////////////////////////////////////////////////////////

    testRead : function () {
      var tempName = fs.join(tempDir, 'test');
      fs.write(tempName, "this is a test file");

      assertEqual("this is a test file", fs.read(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read() 
////////////////////////////////////////////////////////////////////////////////

    testReadMultiline : function () {
      var text = "this is a test file\r\nit contains multiple lines\r\nand\ttabs.";
      var tempName = fs.join(tempDir, 'test');
      fs.write(tempName, text);

      assertEqual(text, fs.read(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read() 
////////////////////////////////////////////////////////////////////////////////

    testReadUnicode : function () {
      var text = "der Möter ging mit viel Geklöter hündert Meter wäiter" +
                 "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기" +
                 "中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服务。" +
                 "Голкипер мадридского «Реала» Икер Касильяс призвал своих партнеров сосредоточить все мысли на предстоящем дерби с «Атлетико»";

      var tempName = fs.join(tempDir, 'test');
      fs.write(tempName, text);

      assertEqual(text, fs.read(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read() 
////////////////////////////////////////////////////////////////////////////////

    testReadEmpty : function () {
      var tempName = fs.join(tempDir, 'test');
      fs.write(tempName, "");

      assertEqual("", fs.read(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read64() 
////////////////////////////////////////////////////////////////////////////////

    testRead64 : function () {
      var tempName = fs.join(tempDir, 'test');
      var data = new Buffer(binary, 'base64');

      // write binary data
      fs.write(tempName, data);

      // read back as base64-encoded
      assertEqual(binary, fs.read64(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read64() 
////////////////////////////////////////////////////////////////////////////////

    testRead64Empty : function () {
      var tempName = fs.join(tempDir, 'test');

      fs.write(tempName, "");

      // read back as base64-encoded
      assertEqual("", fs.read64(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read64() 
////////////////////////////////////////////////////////////////////////////////

    testRead64Short : function () {
      var tempName = fs.join(tempDir, 'test');

      fs.write(tempName, "XXX");

      // read back as base64-encoded
      assertEqual("WFhY", fs.read64(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove() 
////////////////////////////////////////////////////////////////////////////////

    testRemove : function () {
      var tempName = fs.join(tempDir, 'test');

      fs.write(tempName, "abc");

      assertTrue(fs.isFile(tempName));
      fs.remove(tempName);
      assertFalse(fs.isFile(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove() 
////////////////////////////////////////////////////////////////////////////////

    testRemoveNonExisting : function () {
      var tempName = fs.join(tempDir, 'test');

      try {
        fs.remove(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }
      
      // directory
      fs.makeDirectory(tempName);
      
      try {
        fs.remove(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test size() - the filesize of a file
////////////////////////////////////////////////////////////////////////////////

    testSize : function () {
      // create a new file with a specific content
      var tempName = fs.join(tempDir, 'foo');
      fs.write(tempName, "this is a test file");

      // test the size of the new file
      assertEqual(19, fs.size(tempName));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test size() - the filesize of a file
////////////////////////////////////////////////////////////////////////////////

    testSizeNonExisting : function () {
      var tempName = fs.join(tempDir, 'foo');

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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write()
////////////////////////////////////////////////////////////////////////////////

    testWrite : function () {
      var text = "this is a test";
      var filename = fs.join(tempDir, "test");
      assertEqual(true, fs.write(filename, text));
      assertEqual(text.length, fs.size(filename));

      var content = fs.read(filename);
      assertEqual(text, content);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write()
////////////////////////////////////////////////////////////////////////////////

    testWriteEmpty : function () {
      var filename = fs.join(tempDir, "test");
      assertEqual(true, fs.write(filename, ""));
      assertTrue(fs.isFile(filename));

      var content = fs.read(filename);
      assertEqual(0, fs.size(filename));
      assertEqual("", content);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write()
////////////////////////////////////////////////////////////////////////////////

    testWriteJson : function () {
      var filename = fs.join(tempDir, "test");
      assertEqual(true, fs.write(filename, JSON.stringify({
        values: [ 1, 2, 3 ],
        foo: "bar",
        test: true
      })));

      var content = fs.read(filename);
      assertEqual("{\"values\":[1,2,3],\"foo\":\"bar\",\"test\":true}", content);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write()
////////////////////////////////////////////////////////////////////////////////

    testWriteMultiline : function () {
      var text = "the\nquick\r\nbrown\nfoxx\tjumped over\r\n\r\nthe lazy dog.";
      var filename = fs.join(tempDir, "test");
      assertEqual(true, fs.write(filename, text));
      assertEqual(text.length, fs.size(filename));

      var content = fs.read(filename);
      assertEqual(text, content);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write()
////////////////////////////////////////////////////////////////////////////////

    testWriteOverwrite : function () {
      var filename = fs.join(tempDir, "test");
      assertEqual(true, fs.write(filename, "this is a test"));
      
      assertEqual(true, fs.write(filename, "this is another test"));

      var content = fs.read(filename);
      assertEqual("this is another test", content);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write()
////////////////////////////////////////////////////////////////////////////////

    testWriteBuffer : function () {
      var filename = fs.join(tempDir, "test");
      var data = new Buffer(binary, "base64");

      assertEqual(true, fs.write(filename, data));
      assertEqual(binary, fs.read64(filename));
      assertEqual(binary, fs.readFileSync(filename, 'base64'));
      assertEqual(binary, fs.readFile(filename).toString('base64'));
    },

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
