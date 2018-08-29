/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, fail, Buffer */

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


////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function FileSystemSuite () {
  'use strict';
  var ERRORS = require("internal").errors;
  var tempDir;

  var binary = "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";

  var hex = "89504e470d0a1a0a0000000d494844520000003c0000003c08060000003afcd9720000001974455874536f6674776172650041646f626520496d616765526561647971c9653c000005864944415478daec5aeb4f1457143f2cbb2ccf7df07e594194425181150421d6941630184921f2413fd8176db0ff4e3f549b267d24b5a998d8362d9545686c95b2828891375520488102cb2eb02f5edb7bae9dcbccc0126109bb837392c9eebd73ef667e735ebf73ee06b8dd6e78954401af98283dddb85257b72f007e7ef5eacb01fe5fa46cef01dbd2304a5464245caaa9911cd2ebf5f5306b36cb3e2c039601cb8065c0326019b00c58062c03f649b5b45359b4d960f0ef21364e3f7c04c2c3c2f62fe0b9b939f8b5b1918d63a2a2fd0ab0d726fde06107bd38595a5e16dce78fc56b25a7617cf8c6e66666ca168b157a07fa056bbebf590f6fbc9e013a9d16eeb7b5b1f99327f2a405786272928145e183117410dc6ee8e9ef13cce1be0349c990101f2f1d93c6872d7dab64477b719f2fc07a6dd2f90603fcd17a1f5c2e179b43cd151716426c4c0c381c0eaa5d534707acaeaed2fb6ab51af27273a5e9c33dfdfd42b0c9c9507dfe3c68223474acd568203e2e0e82d5c1d0d6fe00ece405e07a7c09d9478f4907f097df7ef32205592c82f9b7cf9c6160f9525450004e97135a4d263a36b6b4407b6727fd5e7bf93dff078c016b33898b89dd743e202000921393d8d8e9747afc0d4951cb65510ee68b6bc9255de2811116afe0e060914ff76dcebe88e977f7aedfc37ddc6f48c28739bf7bdcfd047e6e6860f377efdd236042e07856169b3313aaf9cbeddf60746c8ccd9595946c19b4307737188d10131d051a12f8d01d768b9e7a15a5b332322989e022b5937c1a9bef80d96c86c48404b0d9edf0a4a75b0016d312eee3cbf4ec0c495b6b101ffb220658e7e7a1f37117bb5f5951b16b51dd2bc0f850fcb484e22001e9cfbf5a3dfb32598ffb386a895682da440d7ef2fe0734c03d1b1911ecd1697502ed7381d0a7d4723bc251cb7f2626a0a1c948e72c562b7cf1f55754bbe223dcb6f6769ad3954a25dcfce947389492026f1615ef2d608e5a36fddec2722d06a7fec141c103a32632d2d341afd3b13ccc514b3cbb329102843b0742d034922a14f45a5959a163acaf9f0e3f03955245f3f9f3f171ead7087c4f4dba303f7fc3f7a7c3c370bdfe069bbf78a106d25253e9f7b0d030c1daa0a0203875b28006354e424242208ef832e6748bd502f30b0b6027b1003f112c67d6b7ef34c1a7b51fef7d03800f9a8250a93c8ec56b5183ada6f52a0bfd187d3b2b3313d4e46510f380c9a949e2d3a324f8f5c0c2e2025d9748ace3c2bb55fed1f108230f6dc8ce118cc582c79976871d140410a62d4e728f6743415e1ef5554e0e1e788db8839ed4dac45d8606616d6d0d4ee41aa84ffb05e048bd1ece95976fb9e6dfe969a62dc68088cfa2aff3c172f33aad96fa7b606020053c3c3a0239c77696a6f6bc6b8965e2a26d71c33ce6e788f0708ffb30a723589481a121a8bf758b46f8a5a525ff076cc8c9818c23e9a012f9fbf4ecacc73dd6792baba991b3a37977767551adfb35608ccc15a56550535505e96987058404db44330434078cff22f8bebe1ee4c2b70d58093e94d49483ace040731d1b7f4eeae487904472ac9ef8ad56ab2104670afa060704b57712a1ad6b2435858684fa3e68bdac20a3ea78f468439a42e2323c3a4a5f40745414381ca4769e5aaf9d313a5fbe78694370f37bc0dfddf861c33f6d905060bb172f64681ccde433b7d38452ee14ac4f7c9893b3ef94b202007b5e625f4490a86531af36115ebdddc8ec17806901505c4c1b7f576a6ba1eec38fe839d466ed21ad462b2825bbfb7a7d531e7a2ba74f15d18b6a9a3032a49efc83b873e567e128a199a865aca4d0d42bcacabcaa8d7d0a585cd362b1c0172c3a308da1d4545513b357d0c3399f3500765bd2520f41756525d88826a76766411311c1ee71dd109f763c765bb05a12b77ffca26b29659101cb8065c0326019b00c58062c03de33d9925a6281fed9b56bfb0af07f020c009967495659aa8df80000000049454e44ae426082";

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
      // we need to call join() for the right-hand part too to normalize driver letters on Windows
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

      fs.write(fs.join(tempName, "test"), "this is a test file");

      fs.move(tempName, tempName2);
      assertFalse(fs.isDirectory(tempName));
      assertTrue(fs.isDirectory(tempName2));
      assertTrue(fs.isFile(fs.join(tempName2, "test")));
      assertEqual("this is a test file", fs.read(fs.join(tempName2, "test")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief move()
////////////////////////////////////////////////////////////////////////////////

    testMoveDirectoryExists : function () {
      var tempName, tempName2;

      // create a new file with a specific content
      tempName = fs.join(tempDir, 'foo');
      tempName2 = fs.join(tempDir, 'bar');

      fs.makeDirectory(tempName);
      fs.makeDirectory(tempName2);

      fs.write(fs.join(tempName, "test"), "this is a test file");

      try {
        fs.move(tempName, tempName2);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }

      // nothing moved
      assertTrue(fs.isDirectory(tempName));
      assertTrue(fs.isFile(fs.join(tempName, "test")));
      assertEqual("this is a test file", fs.read(fs.join(tempName, "test")));
      assertTrue(fs.isDirectory(tempName2));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief mtime()
////////////////////////////////////////////////////////////////////////////////

    testMTime : function () {
      var tempName = fs.join(tempDir, 'test');
      fs.write(tempName, "this is a test file");

      var now = Date.now() / 1000;
      var mtime = fs.mtime(tempName);

      // tolerate a max deviation of 60 seconds
      // (deviation needs to be > 1 to make the tests succeed even on busy
      // test servers)
      if (require("internal").platform.substr(0, 3) === 'win') {
        // Windows is bugged. For details see:
        // http://stackoverflow.com/questions/19800811/last-modification-time-reported-by-stat-changes-depending-on-daylight-savings
        // we just work around it here.
        var deltaModulo = Math.abs(mtime - now);
        assertTrue(deltaModulo <= 60 || (deltaModulo > 3540));
      }
      else {
        assertTrue(Math.abs(mtime - now) <= 60);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief mtime()
////////////////////////////////////////////////////////////////////////////////

    testMTimeUpdate : function () {
      var tempName = fs.join(tempDir, 'test');
      fs.write(tempName, "this is a test file");

      var mtime = fs.mtime(tempName);
      internal.wait(2, false);
      fs.write(tempName, "this is an updated test file");

      assertNotEqual(fs.mtime(tempName), mtime);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief mtime()
////////////////////////////////////////////////////////////////////////////////

    testMTimeNonExisting : function () {
      var tempName = fs.join(tempDir, 'test');

      try {
        fs.mtime(tempName);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }
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
/// @brief readBuffer()
////////////////////////////////////////////////////////////////////////////////

    testReadBuffer : function () {
      var tempName = fs.join(tempDir, 'test');
      var data = new Buffer(binary, 'base64');

      // write binary data
      fs.write(tempName, data);

      assertEqual(binary, fs.readBuffer(tempName).toString('base64'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief readFileSync()
////////////////////////////////////////////////////////////////////////////////

    testReadFileSyncBase64 : function () {
      var tempName = fs.join(tempDir, 'test');
      var data = new Buffer(binary, 'base64');

      // write binary data
      fs.write(tempName, data);

      assertEqual(binary, fs.readFileSync(tempName, 'base64'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief readFileSync()
////////////////////////////////////////////////////////////////////////////////

    testReadFileSyncHex : function () {
      var tempName = fs.join(tempDir, 'test');

      // write binary data
      fs.write(tempName, hex);

      assertEqual(hex, fs.readFileSync(tempName, 'ascii'));
      var data = fs.readBuffer(tempName).toString('ascii');
      var data2 = new Buffer(data, 'hex');
      assertEqual(binary, data2.toString('base64'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief readFileSync()
////////////////////////////////////////////////////////////////////////////////

    testReadFileSyncUtf8 : function () {
      var tempName = fs.join(tempDir, 'test');
      fs.write(tempName, "der mötör ging mit großem Geklöter tröten");

      assertEqual("der mötör ging mit großem Geklöter tröten", fs.readFileSync(tempName, 'utf-8'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief readFileSync()
////////////////////////////////////////////////////////////////////////////////

    testReadFileSyncUtf16 : function () {
      var tempName = fs.join(tempDir, 'test');
      var text = "der mötör ging tröten mit Geklöter im größen Gärten";
      var data = new Buffer(text, 'utf16le');

      fs.write(tempName, data);

      var data2 = fs.readFileSync(tempName);
      assertEqual(text, data2.toString('utf16le'));
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
        assertTrue(err.errorNum === ERRORS.ERROR_SYS_ERROR.code ||
                   err.errorNum === ERRORS.ERROR_FORBIDDEN.code);
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
      assertEqual(binary, fs.readBuffer(filename).toString('base64'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test write()
////////////////////////////////////////////////////////////////////////////////

    testWriteBufferHex : function () {
      var filename = fs.join(tempDir, "test");
      var data = new Buffer(binary, "base64");

      assertEqual(true, fs.write(filename, data.toString('hex')));
      assertEqual(hex, fs.read(filename));

      var data3 = fs.read(filename);
      var data2 = new Buffer(data3.toString('ascii'), 'hex');
      assertEqual(binary, data2.toString('base64'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test writeFileSync()
////////////////////////////////////////////////////////////////////////////////

    testWriteFileSync : function () {
      var filename = fs.join(tempDir, "test");
      var data = new Buffer(binary, "base64");

      assertEqual(true, fs.writeFileSync(filename, data));
      assertEqual(binary, fs.read64(filename));
      assertEqual(binary, fs.readFileSync(filename, 'base64'));
      assertEqual(binary, fs.readBuffer(filename).toString('base64'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zipFile()
////////////////////////////////////////////////////////////////////////////////

    testZipFile : function () {
      var tempName = fs.join(tempDir, "out.zip");
      try {
        // outfile must not exist
        fs.remove(tempName);
      }
      catch (err1) {
      }

      fs.write(fs.join(tempDir, "one"), "some input for\r\nfile one");
      fs.write(fs.join(tempDir, "two"), "some input for\r\nfile two");

      assertTrue(fs.zipFile(tempName, tempDir, [ "one", "two" ]));

      fs.remove(fs.join(tempDir, "one"));
      fs.remove(fs.join(tempDir, "two"));

      assertFalse(fs.isFile(fs.join(tempDir, "one")));
      assertFalse(fs.isFile(fs.join(tempDir, "two")));

      assertTrue(fs.unzipFile(tempName, tempDir, false, false));

      assertTrue(fs.isFile(fs.join(tempDir, "one")));
      assertTrue(fs.isFile(fs.join(tempDir, "two")));

      assertEqual(fs.read(fs.join(tempDir, "one")), "some input for\r\nfile one");
      assertEqual(fs.read(fs.join(tempDir, "two")), "some input for\r\nfile two");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zipFile()
////////////////////////////////////////////////////////////////////////////////

    testZipFileOverwrite : function () {
      var tempName = fs.join(tempDir, "out.zip");
      fs.write(tempName, "something");

      fs.write(fs.join(tempDir, "one"), "some input for\r\nfile one");

      try {
        fs.zipFile(tempName, tempDir, [ "one" ]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_CANNOT_OVERWRITE_FILE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zipFile()
////////////////////////////////////////////////////////////////////////////////

    testZipFilePasswordWrong : function () {
      var tempName = fs.join(tempDir, "out.zip");
      try {
        // outfile must not exist
        fs.remove(tempName);
      }
      catch (err1) {
      }

      fs.write(fs.join(tempDir, "one"), "some input for\r\nfile one");
      fs.write(fs.join(tempDir, "two"), "some input for\r\nfile two");

      assertTrue(fs.zipFile(tempName, tempDir, [ "one", "two" ], "SeCR3t"));

      fs.remove(fs.join(tempDir, "one"));
      fs.remove(fs.join(tempDir, "two"));

      assertFalse(fs.isFile(fs.join(tempDir, "one")));
      assertFalse(fs.isFile(fs.join(tempDir, "two")));

      try {
        // wrong password
        fs.unzipFile(tempName, tempDir, false, false, "wrong!");
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_CANNOT_WRITE_FILE.code, err2.errorNum);
      }

      try {
        fs.remove(fs.join(tempDir, "one"));
      }
      catch (e1) {
      }
      try {
        fs.remove(fs.join(tempDir, "two"));
      }
      catch (e2) {
      }

      try {
        // no password
        fs.unzipFile(tempName, tempDir, false, false);
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_CANNOT_WRITE_FILE.code, err3.errorNum);
      }

      try {
        fs.remove(fs.join(tempDir, "one"));
      }
      catch (e3) {
      }
      try {
        fs.remove(fs.join(tempDir, "two"));
      }
      catch (e4) {
      }

      // correct password
      assertTrue(fs.unzipFile(tempName, tempDir, false, false, "SeCR3t"));

      assertTrue(fs.isFile(fs.join(tempDir, "one")));
      assertTrue(fs.isFile(fs.join(tempDir, "two")));

      assertEqual(fs.read(fs.join(tempDir, "one")), "some input for\r\nfile one");
      assertEqual(fs.read(fs.join(tempDir, "two")), "some input for\r\nfile two");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zipFile()
////////////////////////////////////////////////////////////////////////////////

    testZipFileSubDirectory : function () {
      var tempName = fs.join(tempDir, "out.zip");
      try {
        // outfile must not exist
        fs.remove(tempName);
      }
      catch (err1) {
      }

      var subDir = fs.join(tempDir, "files");

      fs.makeDirectory(subDir);
      fs.write(fs.join(subDir, "one"), "some input for\r\nfile one");
      fs.write(fs.join(subDir, "two"), "some input for\r\nfile two");

      assertTrue(fs.zipFile(tempName, tempDir, [ "files/one", "files/two" ]));

      fs.remove(fs.join(subDir, "one"));
      fs.remove(fs.join(subDir, "two"));
      fs.removeDirectory(subDir);

      assertTrue(fs.unzipFile(tempName, tempDir, false, false));

      assertTrue(fs.isDirectory(subDir));
      assertTrue(fs.isFile(fs.join(subDir, "one")));
      assertTrue(fs.isFile(fs.join(subDir, "two")));

      assertEqual(fs.read(fs.join(subDir, "one")), "some input for\r\nfile one");
      assertEqual(fs.read(fs.join(subDir, "two")), "some input for\r\nfile two");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test zipFile()
////////////////////////////////////////////////////////////////////////////////

    testZipFileSubDirectorySkipPaths : function () {
      var tempName = fs.join(tempDir, "out.zip");
      try {
        // outfile must not exist
        fs.remove(tempName);
      }
      catch (err1) {
      }

      var subDir = fs.join(tempDir, "files");

      fs.makeDirectory(subDir);
      fs.write(fs.join(subDir, "one"), "some input for\r\nfile one");
      fs.write(fs.join(subDir, "two"), "some input for\r\nfile two");

      assertTrue(fs.zipFile(tempName, tempDir, [ "files/one", "files/two" ]));

      fs.remove(fs.join(subDir, "one"));
      fs.remove(fs.join(subDir, "two"));
      fs.removeDirectory(subDir);

      assertTrue(fs.unzipFile(tempName, tempDir, true, false));

      assertTrue(fs.isFile(fs.join(tempDir, "one")));
      assertTrue(fs.isFile(fs.join(tempDir, "two")));

      assertEqual(fs.read(fs.join(tempDir, "one")), "some input for\r\nfile one");
      assertEqual(fs.read(fs.join(tempDir, "two")), "some input for\r\nfile two");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test alder32()
////////////////////////////////////////////////////////////////////////////////

    testAdler32EmptyFile : function() {
      const file = fs.join(tempDir, 'empty.txt');
      try {
        fs.remove(file);
      } catch (err) {
      }
      fs.writeFileSync(file, '');
      const checksum = fs.adler32(file);
      assertEqual(checksum, '1');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test alder32()
////////////////////////////////////////////////////////////////////////////////

    testAdler32File : function() {
      const internal = require('internal');
      const file = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'foxx','toomanysecrets.txt'));
      const checksum = fs.adler32(file);
      assertEqual(checksum, '583533794');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test alder32()
////////////////////////////////////////////////////////////////////////////////

    testAdler32NotExistingFile : function() {
      const file = fs.join(tempDir, 'ne.txt');
      try {
        fs.remove(file);
      } catch (err) {
      }
      try {
        fs.adler32(file);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test alder32()
////////////////////////////////////////////////////////////////////////////////

    testAdler32Folder : function() {
      const internal = require('internal');
      const folder = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'foxx'));
      try {
        fs.adler32(folder);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_FILE_NOT_FOUND.code, err.errorNum);
      }
     }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(FileSystemSuite);

return jsunity.done();

