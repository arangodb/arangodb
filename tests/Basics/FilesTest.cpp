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
#include "Basics/files.h"
#include "Basics/operating-system.h"
#include "Basics/system-functions.h"
#include "Random/RandomGenerator.h"

#include "gtest/gtest.h"

#include <absl/strings/str_cat.h>

#include <fcntl.h>
//#include <string.h>
//#include <sys/types.h>
#include <string>
#include <string_view>
#include <vector>

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class FilesTest : public ::testing::Test {
 protected:
  FilesTest() {
    _directory =
        absl::StrCat(TRI_GetTempPath(), TRI_DIR_SEPARATOR_STR, "arangotest-",
                     static_cast<uint64_t>(TRI_microtime()),
                     RandomGenerator::interval(UINT32_MAX));

    long systemError;
    std::string errorMessage;
    auto res =
        TRI_CreateDirectory(_directory.c_str(), systemError, errorMessage);
    EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  }

  ~FilesTest() {
    // let's be sure we delete the right stuff
    TRI_ASSERT(_directory.length() > 10);
    TRI_RemoveDirectory(_directory.c_str());
  }

  std::string writeFile(std::string_view data) {
    std::string filename =
        absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR, "tmp-", ++counter,
                     RandomGenerator::interval(UINT32_MAX));

    FILE* fd = fopen(filename.c_str(), "wb");

    if (fd) {
      [[maybe_unused]] size_t numWritten =
          fwrite(data.data(), data.size(), 1, fd);
      fclose(fd);
    } else {
      EXPECT_TRUE(false == true);
    }

    return filename;
  }

  std::string _directory;
  static uint64_t counter;
};

uint64_t FilesTest::counter = 0;

struct ByteCountFunctor {
  size_t _byteCount;

  ByteCountFunctor() : _byteCount(0) {}

  bool operator()(char const* data, size_t size) {
    _byteCount += size;
    return true;
  };
};  // struct ByteCountFunctor

TEST_F(FilesTest, tst_copyfile) {
  std::string source =
      absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR, "tmp-", ++counter);
  std::string dest = source + "-dest";

  // non-existing file
  std::string error;
  EXPECT_FALSE(TRI_CopyFile(source, dest, error));

  // empty file
  FileUtils::spit(source, std::string(""), false);
  EXPECT_TRUE(TRI_CopyFile(source, dest, error));
  EXPECT_EQ("", FileUtils::slurp(dest));

  // copy over an existing target file
  std::ignore = FileUtils::remove(source);
  FileUtils::spit(source, std::string("foobar"), false);
  EXPECT_FALSE(TRI_CopyFile(source, dest, error));

  std::ignore = FileUtils::remove(source);
  std::ignore = FileUtils::remove(dest);
  FileUtils::spit(source, std::string("foobar"), false);
  EXPECT_TRUE(TRI_CopyFile(source, dest, error));
  EXPECT_EQ("foobar", FileUtils::slurp(dest));

  // copy larger file
  std::string value("the quick brown fox");
  for (size_t i = 0; i < 10; ++i) {
    value += value;
  }

  std::ignore = FileUtils::remove(source);
  std::ignore = FileUtils::remove(dest);
  FileUtils::spit(source, value, false);
  EXPECT_TRUE(TRI_CopyFile(source, dest, error));
  EXPECT_EQ(value, FileUtils::slurp(dest));
  EXPECT_EQ(TRI_SizeFile(source.c_str()), TRI_SizeFile(dest.c_str()));

  // copy file slightly larger than copy buffer
  std::string value2(128 * 1024 + 1, 'x');
  std::ignore = FileUtils::remove(source);
  std::ignore = FileUtils::remove(dest);
  FileUtils::spit(source, value2, false);
  EXPECT_TRUE(TRI_CopyFile(source, dest, error));
  EXPECT_EQ(value2, FileUtils::slurp(dest));
  EXPECT_EQ(TRI_SizeFile(source.c_str()), TRI_SizeFile(dest.c_str()));
}

TEST_F(FilesTest, tst_createdirectory) {
  std::string filename =
      absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR, "tmp-", ++counter);
  long unused1;
  std::string unused2;
  auto res = TRI_CreateDirectory(filename.c_str(), unused1, unused2);
  EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  EXPECT_TRUE(TRI_ExistsFile(filename.c_str()));
  EXPECT_TRUE(TRI_IsDirectory(filename.c_str()));

  res = TRI_RemoveDirectory(filename.c_str());
  EXPECT_FALSE(TRI_ExistsFile(filename.c_str()));
  EXPECT_FALSE(TRI_IsDirectory(filename.c_str()));
}

TEST_F(FilesTest, tst_createdirectoryrecursive) {
  std::string filename1 = absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR,
                                       "tmp-", ++counter, "-dir");
  std::string filename2 = filename1 + TRI_DIR_SEPARATOR_STR + "abc";

  long unused1;
  std::string unused2;
  auto res = TRI_CreateRecursiveDirectory(filename2.c_str(), unused1, unused2);
  EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  EXPECT_TRUE(TRI_ExistsFile(filename1.c_str()));
  EXPECT_TRUE(TRI_IsDirectory(filename1.c_str()));
  EXPECT_TRUE(TRI_ExistsFile(filename2.c_str()));
  EXPECT_TRUE(TRI_IsDirectory(filename2.c_str()));

  res = TRI_RemoveDirectory(filename1.c_str());
  EXPECT_FALSE(TRI_ExistsFile(filename1.c_str()));
  EXPECT_FALSE(TRI_IsDirectory(filename1.c_str()));
  EXPECT_FALSE(TRI_ExistsFile(filename2.c_str()));
  EXPECT_FALSE(TRI_IsDirectory(filename2.c_str()));
}

TEST_F(FilesTest, tst_removedirectorydeterministic) {
  std::string filename1 = absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR,
                                       "tmp-", ++counter, "-dir");
  std::string filename2 = filename1 + TRI_DIR_SEPARATOR_STR + "abc";

  long unused1;
  std::string unused2;
  auto res = TRI_CreateRecursiveDirectory(filename2.c_str(), unused1, unused2);
  EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  EXPECT_TRUE(TRI_ExistsFile(filename1.c_str()));
  EXPECT_TRUE(TRI_IsDirectory(filename1.c_str()));
  EXPECT_TRUE(TRI_ExistsFile(filename2.c_str()));
  EXPECT_TRUE(TRI_IsDirectory(filename2.c_str()));

  res = TRI_RemoveDirectoryDeterministic(filename1.c_str());
  EXPECT_FALSE(TRI_ExistsFile(filename1.c_str()));
  EXPECT_FALSE(TRI_IsDirectory(filename1.c_str()));
  EXPECT_FALSE(TRI_ExistsFile(filename2.c_str()));
  EXPECT_FALSE(TRI_IsDirectory(filename2.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file exists
////////////////////////////////////////////////////////////////////////////////

TEST_F(FilesTest, tst_existsfile) {
  std::string filename = writeFile("");
  EXPECT_TRUE(TRI_ExistsFile(filename.c_str()));
  TRI_UnlinkFile(filename.c_str());
  EXPECT_FALSE(TRI_ExistsFile(filename.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size empty file
////////////////////////////////////////////////////////////////////////////////

TEST_F(FilesTest, tst_filesize_empty) {
  std::string filename = writeFile("");
  EXPECT_EQ(0U, TRI_SizeFile(filename.c_str()));

  TRI_UnlinkFile(filename.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size
////////////////////////////////////////////////////////////////////////////////

TEST_F(FilesTest, tst_filesize_exists) {
  constexpr std::string_view buffer = "the quick brown fox";

  std::string filename = writeFile(buffer);
  EXPECT_EQ(static_cast<int>(buffer.size()), TRI_SizeFile(filename.c_str()));

  TRI_UnlinkFile(filename.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test file size, non existing file
////////////////////////////////////////////////////////////////////////////////

TEST_F(FilesTest, tst_filesize_non) {
  EXPECT_EQ(-1, static_cast<int>(TRI_SizeFile("h5uuuuui3unn645wejhdjhikjdsf")));
  EXPECT_EQ(-1, static_cast<int>(TRI_SizeFile("dihnui8ngiu54")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test absolute path
////////////////////////////////////////////////////////////////////////////////

TEST_F(FilesTest, tst_absolute_paths) {
  std::string path;

#ifdef _WIN32
  path = TRI_GetAbsolutePath("the-fox", "\\tmp");

  EXPECT_EQ(std::string("\\tmp\\the-fox"), path);

  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp");
  EXPECT_EQ(std::string("\\tmp\\the-fox.lol"), path);

  path = TRI_GetAbsolutePath("the-fox.lol", "\\tmp\\the-fox");
  EXPECT_EQ(std::string("\\tmp\\the-fox\\the-fox.lol"), path);

  path = TRI_GetAbsolutePath("file", "\\");
  EXPECT_EQ(std::string("\\file"), path);

  path = TRI_GetAbsolutePath(".\\file", "\\");
  EXPECT_EQ(std::string("\\.\\file"), path);

  path = TRI_GetAbsolutePath("\\file", "\\tmp");
  EXPECT_EQ(std::string("\\tmp\\file"), path);

  path = TRI_GetAbsolutePath("\\file\\to\\file", "\\tmp");
  EXPECT_EQ(std::string("\\tmp\\file\\to\\file"), path);

  path = TRI_GetAbsolutePath("file\\to\\file", "\\tmp");
  EXPECT_EQ(std::string("\\tmp\\file\\to\\file"), path);

  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "abc");
  EXPECT_EQ(std::string("c:\\file\\to\\file"), path);

  path = TRI_GetAbsolutePath("c:\\file\\to\\file", "\\tmp");
  EXPECT_EQ(std::string("c:\\file\\to\\file"), path);

#else

  path = TRI_GetAbsolutePath("the-fox", "/tmp");
  EXPECT_EQ(std::string("/tmp/the-fox"), path);

  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp");
  EXPECT_EQ(std::string("/tmp/the-fox.lol"), path);

  path = TRI_GetAbsolutePath("the-fox.lol", "/tmp/the-fox");
  EXPECT_EQ(std::string("/tmp/the-fox/the-fox.lol"), path);

  path = TRI_GetAbsolutePath("file", "/");
  EXPECT_EQ(std::string("/file"), path);

  path = TRI_GetAbsolutePath("./file", "/");
  EXPECT_EQ(std::string("/./file"), path);

  path = TRI_GetAbsolutePath("/file", "/tmp");
  EXPECT_EQ(std::string("/file"), path);

  path = TRI_GetAbsolutePath("/file/to/file", "/tmp");
  EXPECT_EQ(std::string("/file/to/file"), path);

  path = TRI_GetAbsolutePath("file/to/file", "/tmp");
  EXPECT_EQ(std::string("/tmp/file/to/file"), path);

  path = TRI_GetAbsolutePath("c:file/to/file", "/tmp");
  EXPECT_EQ(std::string("c:file/to/file"), path);
#endif
}

TEST_F(FilesTest, tst_normalize) {
  std::string path;

  path = "/foo/bar/baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_EQ(std::string("\\foo\\bar\\baz"), path);
#else
  EXPECT_EQ(std::string("/foo/bar/baz"), path);
#endif

  path = "\\foo\\bar\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_EQ(std::string("\\foo\\bar\\baz"), path);
#else
  EXPECT_EQ(std::string("\\foo\\bar\\baz"), path);
#endif

  path = "/foo/bar\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_EQ(std::string("\\foo\\bar\\baz"), path);
#else
  EXPECT_EQ(std::string("/foo/bar\\baz"), path);
#endif

  path = "/foo/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_EQ(std::string("\\foo\\bar\\baz"), path);
#else
  EXPECT_EQ(std::string("/foo/bar/\\baz"), path);
#endif

  path = "//foo\\/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_EQ(std::string("\\\\foo\\bar\\baz"), path);
#else
  EXPECT_EQ(std::string("//foo\\/bar/\\baz"), path);
#endif

  path = "\\\\foo\\/bar/\\baz";
  FileUtils::normalizePath(path);
#ifdef _WIN32
  EXPECT_EQ(std::string("\\\\foo\\bar\\baz"), path);
#else
  EXPECT_EQ(std::string("\\\\foo\\/bar/\\baz"), path);
#endif
}

TEST_F(FilesTest, tst_getfilename) {
  EXPECT_EQ("", TRI_GetFilename(""));
  EXPECT_EQ(".", TRI_GetFilename("."));
  EXPECT_EQ("", TRI_GetFilename("/"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("haxxmann"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("/haxxmann"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("/tmp/haxxmann"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("/a/b/c/haxxmann"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("c:/haxxmann"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("c:/tmp/haxxmann"));
  EXPECT_EQ("foo", TRI_GetFilename("c:/tmp/haxxmann/foo"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("\\haxxmann"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("\\a\\haxxmann"));
  EXPECT_EQ("haxxmann", TRI_GetFilename("\\a\\b\\haxxmann"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test TRI_Dirname
////////////////////////////////////////////////////////////////////////////////

TEST_F(FilesTest, tst_dirname) {
#ifdef _WIN32
  EXPECT_EQ("C:\\Users\\abc def\\foobar",
            TRI_Dirname("C:\\Users\\abc def\\foobar\\"));
  EXPECT_EQ("C:\\Users\\abc def\\foobar",
            TRI_Dirname("C:\\Users\\abc def\\foobar\\baz"));
  EXPECT_EQ("C:\\Users\\abc def\\foobar",
            TRI_Dirname("C:\\Users\\abc def\\foobar\\baz.text"));
  EXPECT_EQ("C:\\Users\\abc def\\foobar",
            TRI_Dirname("C:\\Users\\abc def\\foobar\\VERSION-1.tmp"));
  EXPECT_EQ("\\Users\\abc def\\foobar",
            TRI_Dirname("\\Users\\abc def\\foobar\\VERSION-1.tmp"));
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

TEST_F(FilesTest, tst_processFile) {
  constexpr std::string_view buffer = "the quick brown fox";

  std::string filename = writeFile(buffer);

  ByteCountFunctor bcf;
  auto reader = std::ref(bcf);
  bool good = TRI_ProcessFile(filename.c_str(), reader);

  EXPECT_TRUE(good);
  EXPECT_EQ(buffer.size(), bcf._byteCount);

  TRI_SHA256Functor sha;
  auto shaReader = std::ref(sha);

  good = TRI_ProcessFile(filename.c_str(), shaReader);

  EXPECT_TRUE(good);
  EXPECT_TRUE(
      sha.finalize().compare(
          "9ecb36561341d18eb65484e833efea61edc74b84cf5e6ae1b81c63533e25fc8f") ==
      0);

  TRI_UnlinkFile(filename.c_str());
}

TEST_F(FilesTest, tst_readpointer) {
  constexpr std::string_view buffer =
      "some random garbled stuff...\nabc\tabignndnf";
  std::string filename = writeFile(buffer);

  {
    // buffer big enough
    int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);

    char result[100];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, static_cast<TRI_read_return_t>(buffer.size()));
    EXPECT_EQ(0, strncmp(buffer.data(), &result[0], buffer.size()));

    TRI_CLOSE(fd);
  }

  {
    // read multiple times
    int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);

    char result[10];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 10);
    EXPECT_EQ(0, strncmp(buffer.data(), &result[0], 10));

    numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 10);
    EXPECT_EQ(0, strncmp(buffer.data() + 10, &result[0], 10));

    numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 10);
    EXPECT_EQ(0, strncmp(buffer.data() + 20, &result[0], 10));

    TRI_CLOSE(fd);
  }

  {
    // buffer way too small
    int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);

    char result[5];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 5);
    EXPECT_EQ(0, strncmp(buffer.data(), &result[0], 5));

    TRI_CLOSE(fd);
  }

  {
    // buffer way too small
    int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);
    EXPECT_GE(fd, 0);

    char result[1];
    TRI_read_return_t numRead = TRI_ReadPointer(fd, &result[0], sizeof(result));
    EXPECT_EQ(numRead, 1);
    EXPECT_EQ(0, strncmp(buffer.data(), &result[0], 1));

    TRI_CLOSE(fd);
  }

  TRI_UnlinkFile(filename.c_str());
}

TEST_F(FilesTest, tst_listfiles) {
  constexpr std::string_view content = "piffpaffpuff";

  std::vector<std::string> names;
  constexpr size_t n = 16;
  // create subdirs
  for (size_t i = 0; i < n; ++i) {
    std::string name =
        absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR, "tmp-", ++counter);
    long unused1;
    std::string unused2;
    auto res = TRI_CreateDirectory(name.c_str(), unused1, unused2);
    EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
    names.push_back(TRI_Basename(name.c_str()));
  }

  // create a few files on top
  for (size_t i = 0; i < 5; ++i) {
    std::string name =
        absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR, "tmp-", ++counter);
    FileUtils::spit(name, content.data(), content.size(), false);
    names.push_back(TRI_Basename(name.c_str()));
  }
  std::sort(names.begin(), names.end());

  auto found = FileUtils::listFiles(_directory);
  EXPECT_EQ(n + 5, found.size());

  std::sort(found.begin(), found.end());
  EXPECT_EQ(names, found);
}

TEST_F(FilesTest, tst_countfiles) {
  constexpr std::string_view content = "piffpaffpuff";

  constexpr size_t n = 16;
  // create subdirs
  for (size_t i = 0; i < n; ++i) {
    std::string name =
        absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR, "tmp-", ++counter);
    long unused1;
    std::string unused2;
    auto res = TRI_CreateDirectory(name.c_str(), unused1, unused2);
    EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  }
  // create a few files on top
  for (size_t i = 0; i < 5; ++i) {
    std::string name =
        absl::StrCat(_directory, TRI_DIR_SEPARATOR_STR, "tmp-", ++counter);
    FileUtils::spit(name, content.data(), content.size(), false);
  }

  size_t found = FileUtils::countFiles(_directory);
  EXPECT_EQ(n + 5, found);
}
