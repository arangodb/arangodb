////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Basics/operating-system.h"
#include "Basics/system-functions.h"
#include "Random/RandomGenerator.h"

#include "gtest/gtest.h"
#include <string>
#include <iostream>
#include <fcntl.h>

using namespace arangodb::basics;

static bool Initialized = false;
static uint64_t counter = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class CFilesTest : public ::testing::Test {
protected:
  CFilesTest () : _directory(true) {
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

  ~CFilesTest () {
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
      EXPECT_TRUE(false == true);
    }

    return filename;
  }

  StringBuffer _directory;
};


struct ByteCountFunctor {

  size_t _byteCount;

  ByteCountFunctor() : _byteCount(0) {};

  bool operator() (const char * data, size_t size) {
    _byteCount+=size;
    return true;
  };
};// struct ByteCountFunctor

TEST_F(CFilesTest, tst_copyfile) {
  std::ostringstream out;
  out << _directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter;
  
  std::string source = out.str();
  out << "-dest";
  std::string dest = out.str();

  // non-existing file
  std::string error;
  EXPECT_TRUE(false == TRI_CopyFile(source, dest, error));

  // empty file
  FileUtils::spit(source, std::string(""), false);
  EXPECT_TRUE(true == TRI_CopyFile(source, dest, error));
  EXPECT_TRUE("" == FileUtils::slurp(dest));

  // copy over an existing target file
  std::ignore = FileUtils::remove(source);
  FileUtils::spit(source, std::string("foobar"), false);
  EXPECT_TRUE(false == TRI_CopyFile(source, dest, error));
  
  std::ignore = FileUtils::remove(source);
  std::ignore = FileUtils::remove(dest);
  FileUtils::spit(source, std::string("foobar"), false);
  EXPECT_TRUE(true == TRI_CopyFile(source, dest, error));
  EXPECT_TRUE("foobar" == FileUtils::slurp(dest));

  // copy larger file
  std::string value("the quick brown fox");
  for (size_t i = 0; i < 10; ++i) {
    value += value;
  }
 
  std::ignore = FileUtils::remove(source);
  std::ignore = FileUtils::remove(dest);
  FileUtils::spit(source, value, false);
  EXPECT_TRUE(true == TRI_CopyFile(source, dest, error));
  EXPECT_TRUE(value == FileUtils::slurp(dest));
  EXPECT_TRUE(TRI_SizeFile(source.c_str()) == TRI_SizeFile(dest.c_str()));

  // copy file slightly larger than copy buffer
  std::string value2(128 * 1024 + 1, 'x');
  std::ignore = FileUtils::remove(source);
  std::ignore = FileUtils::remove(dest);
  FileUtils::spit(source, value2, false);
  EXPECT_TRUE(true == TRI_CopyFile(source, dest, error));
  EXPECT_TRUE(value2 == FileUtils::slurp(dest));
  EXPECT_TRUE(TRI_SizeFile(source.c_str()) == TRI_SizeFile(dest.c_str()));
}

TEST_F(CFilesTest, tst_createdirectory) {
  std::ostringstream out;
  out << _directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter << "-dir";

  std::string filename = out.str();
  long unused1;
  std::string unused2;
  auto res = TRI_CreateDirectory(filename.c_str(), unused1, unused2);
  EXPECT_TRUE(TRI_ERROR_NO_ERROR == res);
  EXPECT_TRUE(true == TRI_ExistsFile(filename.c_str()));
  EXPECT_TRUE(true == TRI_IsDirectory(filename.c_str()));

  res = TRI_RemoveDirectory(filename.c_str());
  EXPECT_TRUE(false == TRI_ExistsFile(filename.c_str()));
  EXPECT_TRUE(false == TRI_IsDirectory(filename.c_str()));
}

TEST_F(CFilesTest, tst_createdirectoryrecursive) {
  std::ostringstream out;
  out << _directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter << "-dir";
  
  std::string filename1 = out.str();
  out << TRI_DIR_SEPARATOR_CHAR << "abc";
  std::string filename2 = out.str();

  long unused1;
  std::string unused2;
  auto res = TRI_CreateRecursiveDirectory(filename2.c_str(), unused1, unused2);
  EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  EXPECT_TRUE(true == TRI_ExistsFile(filename1.c_str()));
  EXPECT_TRUE(true == TRI_IsDirectory(filename1.c_str()));
  EXPECT_TRUE(true == TRI_ExistsFile(filename2.c_str()));
  EXPECT_TRUE(true == TRI_IsDirectory(filename2.c_str()));

  res = TRI_RemoveDirectory(filename1.c_str());
  EXPECT_TRUE(false == TRI_ExistsFile(filename1.c_str()));
  EXPECT_TRUE(false == TRI_IsDirectory(filename1.c_str()));
  EXPECT_TRUE(false == TRI_ExistsFile(filename2.c_str()));
  EXPECT_TRUE(false == TRI_IsDirectory(filename2.c_str()));
}

TEST_F(CFilesTest, tst_removedirectorydeterministic) {
  std::ostringstream out;
  out << _directory.c_str() << TRI_DIR_SEPARATOR_CHAR << "tmp-" << ++counter << "-dir";
  
  std::string filename1 = out.str();
  out << TRI_DIR_SEPARATOR_CHAR << "abc";
  std::string filename2 = out.str();

  long unused1;
  std::string unused2;
  auto res = TRI_CreateRecursiveDirectory(filename2.c_str(), unused1, unused2);
  EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  EXPECT_TRUE(true == TRI_ExistsFile(filename1.c_str()));
  EXPECT_TRUE(true == TRI_IsDirectory(filename1.c_str()));
  EXPECT_TRUE(true == TRI_ExistsFile(filename2.c_str()));
  EXPECT_TRUE(true == TRI_IsDirectory(filename2.c_str()));

  res = TRI_RemoveDirectoryDeterministic(filename1.c_str());
  EXPECT_TRUE(false == TRI_ExistsFile(filename1.c_str()));
  EXPECT_TRUE(false == TRI_IsDirectory(filename1.c_str()));
  EXPECT_TRUE(false == TRI_ExistsFile(filename2.c_str()));
  EXPECT_TRUE(false == TRI_IsDirectory(filename2.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file exists
////////////////////////////////////////////////////////////////////////////////

TEST_F(CFilesTest, tst_existsfile) {
  StringBuffer* filename = writeFile("");
  EXPECT_TRUE(true == TRI_ExistsFile(filename->c_str()));
  TRI_UnlinkFile(filename->c_str());
  EXPECT_TRUE(false == TRI_ExistsFile(filename->c_str()));

  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size empty file
////////////////////////////////////////////////////////////////////////////////

TEST_F(CFilesTest, tst_filesize_empty) {
  StringBuffer* filename = writeFile("");
  EXPECT_TRUE(0U == TRI_SizeFile(filename->c_str()));

  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size
////////////////////////////////////////////////////////////////////////////////

TEST_F(CFilesTest, tst_filesize_exists) {
  const char* buffer = "the quick brown fox";
  
  StringBuffer* filename = writeFile(buffer);
  EXPECT_TRUE(static_cast<int>(strlen(buffer)) == TRI_SizeFile(filename->c_str()));

  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size, non existing file
////////////////////////////////////////////////////////////////////////////////

TEST_F(CFilesTest, tst_filesize_non) {
  EXPECT_TRUE(-1 == (int) TRI_SizeFile("h5uuuuui3unn645wejhdjhikjdsf"));
  EXPECT_TRUE(-1 == (int) TRI_SizeFile("dihnui8ngiu54"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test absolute path
////////////////////////////////////////////////////////////////////////////////

TEST_F(CFilesTest, tst_absolute_paths) {
  std::string path;

#ifdef _WIN32
  path = TRI_GetAbsolutePath("the-fox", "\\tmp");

  EXPECT_TRUE(std::string("\\tmp\\the-fox") == path);

  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp");
  EXPECT_TRUE(std::string("\\tmp\\the-fox.lol") == path);
  
  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp\\the-fox");
  EXPECT_TRUE(std::string("\\tmp\\the-fox\\the-fox.lol") == path);
  
  path = TRI_GetAbsolutePath("file", "\\");
  EXPECT_TRUE(std::string("\\file") == path);
  
  path = TRI_GetAbsolutePath(".\\file", "\\");
  EXPECT_TRUE(std::string("\\.\\file") == path);
  
  path = TRI_GetAbsolutePath("\\file", "\\tmp");
  EXPECT_TRUE(std::string("\\tmp\\file") == path);
  
  path = TRI_GetAbsolutePath("\\file\\to\\file", "\\tmp");
  EXPECT_TRUE(std::string("\\tmp\\file\\to\\file") == path);
  
  path = TRI_GetAbsolutePath("file\\to\\file", "\\tmp");
  EXPECT_TRUE(std::string("\\tmp\\file\\to\\file") == path);
  
  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "abc");
  EXPECT_TRUE(std::string("c:\\file\\to\\file") == path);
  
  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "\\tmp");
  EXPECT_TRUE(std::string("c:\\file\\to\\file") == path);

#else

  path = TRI_GetAbsolutePath("the-fox", "/tmp");
  EXPECT_TRUE(std::string("/tmp/the-fox") == path);

  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp");
  EXPECT_TRUE(std::string("/tmp/the-fox.lol") == path);
  
  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp/the-fox");
  EXPECT_TRUE(std::string("/tmp/the-fox/the-fox.lol") == path);
  
  path = TRI_GetAbsolutePath("file", "/");
  EXPECT_TRUE(std::string("/file") == path);
  
  path = TRI_GetAbsolutePath("./file", "/");
  EXPECT_TRUE(std::string("/./file") == path);
  
  path = TRI_GetAbsolutePath("/file", "/tmp");
  EXPECT_TRUE(std::string("/file") == path);
  
  path = TRI_GetAbsolutePath("/file/to/file", "/tmp");
  EXPECT_TRUE(std::string("/file/to/file") == path);
  
  path = TRI_GetAbsolutePath("file/to/file", "/tmp");
  EXPECT_TRUE(std::string("/tmp/file/to/file") == path);
  
  path = TRI_GetAbsolutePath("c:file/to/file", "/tmp");
  EXPECT_TRUE(std::string("c:file/to/file") == path);
#endif
}

TEST_F(CFilesTest, tst_normalize) {
  std::string path;

  path = "/foo/bar/baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_TRUE(std::string("\\foo\\bar\\baz") == path);
#else
  EXPECT_TRUE(std::string("/foo/bar/baz") == path);
#endif

  path = "\\foo\\bar\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_TRUE(std::string("\\foo\\bar\\baz") == path);
#else
  EXPECT_TRUE(std::string("\\foo\\bar\\baz") == path);
#endif

  path = "/foo/bar\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_TRUE(std::string("\\foo\\bar\\baz") == path);
#else
  EXPECT_TRUE(std::string("/foo/bar\\baz") == path);
#endif

  path = "/foo/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_TRUE(std::string("\\foo\\bar\\baz") == path);
#else
  EXPECT_TRUE(std::string("/foo/bar/\\baz") == path);
#endif

  path = "//foo\\/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_TRUE(std::string("\\\\foo\\bar\\baz") == path);
#else
  EXPECT_TRUE(std::string("//foo\\/bar/\\baz") == path);
#endif

  path = "\\\\foo\\/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_TRUE(std::string("\\\\foo\\bar\\baz") == path);
#else
  EXPECT_TRUE(std::string("\\\\foo\\/bar/\\baz") == path);
#endif
}

TEST_F(CFilesTest, tst_getfilename) {
  EXPECT_TRUE("" == TRI_GetFilename(""));
  EXPECT_TRUE("." == TRI_GetFilename("."));
  EXPECT_TRUE("" == TRI_GetFilename("/"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("haxxmann"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("/haxxmann"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("/tmp/haxxmann"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("/a/b/c/haxxmann"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("c:/haxxmann"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("c:/tmp/haxxmann"));
  EXPECT_TRUE("foo" == TRI_GetFilename("c:/tmp/haxxmann/foo"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("\\haxxmann"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("\\a\\haxxmann"));
  EXPECT_TRUE("haxxmann" == TRI_GetFilename("\\a\\b\\haxxmann"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test TRI_Dirname
////////////////////////////////////////////////////////////////////////////////

TEST_F(CFilesTest, tst_dirname) {
#ifdef _WIN32
  EXPECT_EQ("C:\\Users\\abc def\\foobar", TRI_Dirname("C:\\Users\\abc def\\foobar\\"));
  EXPECT_EQ("C:\\Users\\abc def\\foobar", TRI_Dirname("C:\\Users\\abc def\\foobar\\baz"));
  EXPECT_EQ("C:\\Users\\abc def\\foobar", TRI_Dirname("C:\\Users\\abc def\\foobar\\baz.text"));
  EXPECT_EQ("C:\\Users\\abc def\\foobar", TRI_Dirname("C:\\Users\\abc def\\foobar\\VERSION-1.tmp"));
  EXPECT_EQ("\\Users\\abc def\\foobar", TRI_Dirname("\\Users\\abc def\\foobar\\VERSION-1.tmp"));
#else 
  EXPECT_EQ("/tmp/abc/def hihi", TRI_Dirname("/tmp/abc/def hihi/"));
  EXPECT_EQ("/tmp/abc/def hihi", TRI_Dirname("/tmp/abc/def hihi/abc"));
  EXPECT_EQ("/tmp/abc/def hihi", TRI_Dirname("/tmp/abc/def hihi/abc.txt"));
  EXPECT_EQ("/tmp", TRI_Dirname("/tmp/"));
  EXPECT_EQ("/tmp", TRI_Dirname("/tmp/1"));
  EXPECT_EQ("/", TRI_Dirname("/tmp"));
  EXPECT_EQ("/", TRI_Dirname("/"));
  EXPECT_EQ(".", TRI_Dirname("./"));
  EXPECT_EQ(".", TRI_Dirname(""));
  EXPECT_EQ(".", TRI_Dirname("."));
  EXPECT_EQ("..", TRI_Dirname(".."));
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process data in a file via a functor
////////////////////////////////////////////////////////////////////////////////

TEST_F(CFilesTest, tst_processFile) {
  const char* buffer = "the quick brown fox";
  bool good;

  StringBuffer* filename = writeFile(buffer);

  ByteCountFunctor bcf;
  auto reader = std::ref(bcf);
  good = TRI_ProcessFile(filename->c_str(), reader);

  EXPECT_TRUE(good);
  EXPECT_EQ(strlen(buffer), bcf._byteCount);

  TRI_SHA256Functor sha;
  auto shaReader = std::ref(sha);

  good = TRI_ProcessFile(filename->c_str(), shaReader);

  EXPECT_TRUE(good);
  EXPECT_TRUE(sha.finalize().compare("9ecb36561341d18eb65484e833efea61edc74b84cf5e6ae1b81c63533e25fc8f") == 0);

  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

TEST_F(CFilesTest, tst_readpointer) {
  char const* buffer = "some random garbled stuff...\nabc\tabignndnf";
  StringBuffer* filename = writeFile(buffer);

  {
    // buffer big enough
    int fd = TRI_OPEN(filename->c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);
  
    char result[100];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, static_cast<TRI_read_return_t>(strlen(buffer)));
    EXPECT_EQ(0, strncmp(buffer, &result[0], strlen(buffer)));
  
    TRI_CLOSE(fd);
  }
  
  {
    // read multiple times
    int fd = TRI_OPEN(filename->c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);

    char result[10];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 10);
    EXPECT_EQ(0, strncmp(buffer, &result[0], 10));
    
    numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 10);
    EXPECT_EQ(0, strncmp(buffer + 10, &result[0], 10));
    
    numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 10);
    EXPECT_EQ(0, strncmp(buffer + 20, &result[0], 10));
    
    TRI_CLOSE(fd);
  }
  
  {
    // buffer way too small
    int fd = TRI_OPEN(filename->c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);

    char result[5];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 5);
    EXPECT_EQ(0, strncmp(buffer, &result[0], 5));
    
    TRI_CLOSE(fd);
  }
  
  {
    // buffer way too small
    int fd = TRI_OPEN(filename->c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);

    char result[1];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 1);
    EXPECT_EQ(0, strncmp(buffer, &result[0], 1));
    
    TRI_CLOSE(fd);
  }
  
  TRI_UnlinkFile(filename->c_str());
  delete filename;
}

