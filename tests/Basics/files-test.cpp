///////////////////////////////////////////////////////////////////////////////
/// @brief test suite for files.c
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Random/RandomGenerator.h"

#include "catch.hpp"
#include <string>
#include <iostream>

using namespace arangodb::basics;

static bool Initialized = false;
static uint64_t counter = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CFilesSetup {
  CFilesSetup () : _directory(true) {
    long systemError;
    std::string errorMessage;
    
    if (!Initialized) {
      Initialized = true;
      arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    }

    _directory.appendText(TRI_GetTempPath());
    _directory.appendChar(TRI_DIR_SEPARATOR_CHAR);
    _directory.appendText("arangotest-");
    _directory.appendInteger(static_cast<uint64_t>(TRI_microtime()));
    _directory.appendInteger(arangodb::RandomGenerator::interval(UINT32_MAX));

    TRI_CreateDirectory(_directory.c_str(), systemError, errorMessage);
  }

  ~CFilesSetup () {
    // let's be sure we delete the right stuff
    TRI_ASSERT(_directory.length() > 10);

    TRI_RemoveDirectory(_directory.c_str());
  }

  StringBuffer* writeFile (const char* blob) {
    StringBuffer* filename = new StringBuffer(true);
    filename->appendText(_directory);
    filename->appendChar(TRI_DIR_SEPARATOR_CHAR);
    filename->appendText("tmp-");
    filename->appendInteger(++counter);
    filename->appendInteger(arangodb::RandomGenerator::interval(UINT32_MAX));

    FILE* fd = fopen(filename->c_str(), "wb");

    if (fd) {
      size_t numWritten = fwrite(blob, strlen(blob), 1, fd);
      (void) numWritten;
      fclose(fd);
    }
    else {
      CHECK(false == true);
    }

    return filename;
  }

  StringBuffer _directory;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CFilesTest", "[files]") {
  CFilesSetup s;

SECTION("tst_copyfile") {
  std::ostringstream out;
  out << s._directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter;
  
  std::string source = out.str();
  out << "-dest";
  std::string dest = out.str();

  // non-existing file
  std::string error;
  CHECK(false == TRI_CopyFile(source, dest, error));

  // empty file
  FileUtils::spit(source, std::string(""), false);
  CHECK(true == TRI_CopyFile(source, dest, error));
  CHECK("" == FileUtils::slurp(dest));

  // copy over an existing target file
  FileUtils::remove(source);
  FileUtils::spit(source, std::string("foobar"), false);
  CHECK(false == TRI_CopyFile(source, dest, error));
  
  FileUtils::remove(source);
  FileUtils::remove(dest);
  FileUtils::spit(source, std::string("foobar"), false);
  CHECK(true == TRI_CopyFile(source, dest, error));
  CHECK("foobar" == FileUtils::slurp(dest));

  // copy larger file
  std::string value("the quick brown fox");
  for (size_t i = 0; i < 10; ++i) {
    value += value;
  }
 
  FileUtils::remove(source);
  FileUtils::remove(dest);
  FileUtils::spit(source, value, false);
  CHECK(true == TRI_CopyFile(source, dest, error));
  CHECK(value == FileUtils::slurp(dest));
  CHECK(TRI_SizeFile(source.c_str()) == TRI_SizeFile(dest.c_str()));

  // copy file slightly larger than copy buffer
  std::string value2(128 * 1024 + 1, 'x');
  FileUtils::remove(source);
  FileUtils::remove(dest);
  FileUtils::spit(source, value2, false);
  CHECK(true == TRI_CopyFile(source, dest, error));
  CHECK(value2 == FileUtils::slurp(dest));
  CHECK(TRI_SizeFile(source.c_str()) == TRI_SizeFile(dest.c_str()));
}

SECTION("tst_createdirectory") {
  std::ostringstream out;
  out << s._directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter << "-dir";

  std::string filename = out.str();
  long unused1;
  std::string unused2;
  int res = TRI_CreateDirectory(filename.c_str(), unused1, unused2);
  CHECK(0 == res);
  CHECK(true == TRI_ExistsFile(filename.c_str()));
  CHECK(true == TRI_IsDirectory(filename.c_str()));

  res = TRI_RemoveDirectory(filename.c_str());
  CHECK(false == TRI_ExistsFile(filename.c_str()));
  CHECK(false == TRI_IsDirectory(filename.c_str()));
}

SECTION("tst_createdirectoryrecursive") {
  std::ostringstream out;
  out << s._directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter << "-dir";
  
  std::string filename1 = out.str();
  out << TRI_DIR_SEPARATOR_CHAR << "abc";
  std::string filename2 = out.str();

  long unused1;
  std::string unused2;
  int res = TRI_CreateRecursiveDirectory(filename2.c_str(), unused1, unused2);
  CHECK(0 == res);
  CHECK(true == TRI_ExistsFile(filename1.c_str()));
  CHECK(true == TRI_IsDirectory(filename1.c_str()));
  CHECK(true == TRI_ExistsFile(filename2.c_str()));
  CHECK(true == TRI_IsDirectory(filename2.c_str()));

  res = TRI_RemoveDirectory(filename1.c_str());
  CHECK(false == TRI_ExistsFile(filename1.c_str()));
  CHECK(false == TRI_IsDirectory(filename1.c_str()));
  CHECK(false == TRI_ExistsFile(filename2.c_str()));
  CHECK(false == TRI_IsDirectory(filename2.c_str()));
}

SECTION("tst_removedirectorydeterministic") {
  std::ostringstream out;
  out << s._directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter << "-dir";
  
  std::string filename1 = out.str();
  out << TRI_DIR_SEPARATOR_CHAR << "abc";
  std::string filename2 = out.str();

  long unused1;
  std::string unused2;
  int res = TRI_CreateRecursiveDirectory(filename2.c_str(), unused1, unused2);
  CHECK(0 == res);
  CHECK(true == TRI_ExistsFile(filename1.c_str()));
  CHECK(true == TRI_IsDirectory(filename1.c_str()));
  CHECK(true == TRI_ExistsFile(filename2.c_str()));
  CHECK(true == TRI_IsDirectory(filename2.c_str()));

  res = TRI_RemoveDirectoryDeterministic(filename1.c_str());
  CHECK(false == TRI_ExistsFile(filename1.c_str()));
  CHECK(false == TRI_IsDirectory(filename1.c_str()));
  CHECK(false == TRI_ExistsFile(filename2.c_str()));
  CHECK(false == TRI_IsDirectory(filename2.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file exists
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_existsfile") {
  StringBuffer* filename = s.writeFile("");
  CHECK(true == TRI_ExistsFile(filename->c_str()));
  TRI_UnlinkFile(filename->c_str());
  CHECK(false == TRI_ExistsFile(filename->c_str()));

  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size empty file
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_filesize_empty") {
  StringBuffer* filename = s.writeFile("");
  CHECK(0 == (int) TRI_SizeFile(filename->c_str()));

  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_filesize_exists") {
  const char* buffer = "the quick brown fox";
  
  StringBuffer* filename = s.writeFile(buffer);
  CHECK((int) strlen(buffer) == (int) TRI_SizeFile(filename->c_str()));

  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size, non existing file
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_filesize_non") {
  CHECK((int) -1 == (int) TRI_SizeFile("h5uuuuui3unn645wejhdjhikjdsf"));
  CHECK((int) -1 == (int) TRI_SizeFile("dihnui8ngiu54"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test absolute path
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_absolute_paths") {
  char* path;

#ifdef _WIN32
  path = TRI_GetAbsolutePath("the-fox", "\\tmp");

  CHECK(std::string("\\tmp\\the-fox") == path);
  TRI_Free(path);

  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp");
  CHECK(std::string("\\tmp\\the-fox.lol") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp\\the-fox");
  CHECK(std::string("\\tmp\\the-fox\\the-fox.lol") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("file", "\\");
  CHECK(std::string("\\file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath(".\\file", "\\");
  CHECK(std::string("\\.\\file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("\\file", "\\tmp");
  CHECK(std::string("\\tmp\\file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("\\file\\to\\file", "\\tmp");
  CHECK(std::string("\\tmp\\file\\to\\file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("file\\to\\file", "\\tmp");
  CHECK(std::string("\\tmp\\file\\to\\file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "abc");
  CHECK(std::string("c:\\file\\to\\file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "\\tmp");
  CHECK(std::string("c:\\file\\to\\file") == path);
  TRI_Free(path);

#else

  path = TRI_GetAbsolutePath("the-fox", "/tmp");
  CHECK(std::string("/tmp/the-fox") == path);
  TRI_Free(path);

  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp");
  CHECK(std::string("/tmp/the-fox.lol") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp/the-fox");
  CHECK(std::string("/tmp/the-fox/the-fox.lol") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("file", "/");
  CHECK(std::string("/file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("./file", "/");
  CHECK(std::string("/./file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("/file", "/tmp");
  CHECK(std::string("/file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("/file/to/file", "/tmp");
  CHECK(std::string("/file/to/file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("file/to/file", "/tmp");
  CHECK(std::string("/tmp/file/to/file") == path);
  TRI_Free(path);
  
  path = TRI_GetAbsolutePath("c:file/to/file", "/tmp");
  CHECK(std::string("c:file/to/file") == path);
  TRI_Free(path);
#endif
}

SECTION("tst_normalize") {
  std::string path;

  path = "/foo/bar/baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  CHECK(std::string("\\foo\\bar\\baz") == path);
#else
  CHECK(std::string("/foo/bar/baz") == path);
#endif

  path = "\\foo\\bar\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  CHECK(std::string("\\foo\\bar\\baz") == path);
#else
  CHECK(std::string("\\foo\\bar\\baz") == path);
#endif
  
  path = "/foo/bar\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  CHECK(std::string("\\foo\\bar\\baz") == path);
#else
  CHECK(std::string("/foo/bar\\baz") == path);
#endif
  
  path = "/foo/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  CHECK(std::string("\\foo\\bar\\baz") == path);
#else
  CHECK(std::string("/foo/bar/\\baz") == path);
#endif
  
  path = "//foo\\/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  CHECK(std::string("\\\\foo\\bar\\baz") == path);
#else
  CHECK(std::string("//foo\\/bar/\\baz") == path);
#endif
  
  path = "\\\\foo\\/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  CHECK(std::string("\\\\foo\\bar\\baz") == path);
#else
  CHECK(std::string("\\\\foo\\/bar/\\baz") == path);
#endif
}

}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
