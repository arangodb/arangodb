////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/FileUtils.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "RocksDBEngine/RocksDBChecksumEnv.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace arangodb;

struct FileInfo {
  std::string name;
  std::size_t size;
  FileInfo(std::string const& n, std::size_t s) : name(n), size(s) {}
};

class FileMaker {
 public:
  FileMaker(std::string const& directory, std::vector<FileInfo> const& files) {
    std::vector<char> p;
    for (auto const& fi : files) {
      std::string path = basics::FileUtils::buildFilename(directory, fi.name);
      p.clear();
      p.resize(fi.size);
      for (size_t i = 0; i < fi.size; ++i) {
        p[i] = i % 67 + '!';
      }
      basics::FileUtils::spit(path, p.data(), fi.size);
    }
  }
};

struct RocksDBChecksumHelperTest : public ::testing::Test {
  RocksDBChecksumHelperTest() {
    long sysError;
    std::string errMsg;
    ErrorCode res = TRI_GetTempName("", directory, false, sysError, errMsg);
    EXPECT_EQ(res, ErrorCode{TRI_ERROR_NO_ERROR});
    res = TRI_CreateDirectory(directory.c_str(), sysError, errMsg);
    EXPECT_EQ(res, ErrorCode{TRI_ERROR_NO_ERROR});
  }

  ~RocksDBChecksumHelperTest() {
    ErrorCode res = TRI_RemoveDirectory(directory.c_str());
    EXPECT_EQ(res, ErrorCode{TRI_ERROR_NO_ERROR});
  }

  std::string directory;
};

TEST_F(RocksDBChecksumHelperTest, test_empty) {
  checksum::ChecksumHelper helper(directory);
  helper.checkMissingShaFiles();

  // no additional files should have been produced
  auto files = TRI_FilesDirectory(directory.c_str());
  ASSERT_TRUE(files.empty());
}

TEST_F(RocksDBChecksumHelperTest, test_no_missing_files) {
  FileMaker(directory, {
                           {"CURRENT", 16},
                           {"MANIFEST-000005", 10000},
                           {"OPTIONS-000041", 20000},
                           {"OPTIONS-000043", 20000},
                           {"000050.sha."
                            "16f1e6b2f6b7cedcc122807551c5d04a0e339e4b406879006e"
                            "ccb580ef6d3545.hash",
                            0},
                           {"000050.sst", 32000000},
                           {"000051.sha."
                            "908e00481d4913afdc583633624dd87176af80558cc3c2a8c5"
                            "528ab89e9d5c60.hash",
                            0},
                           {"000051.sst", 48000000},
                           {"000053.sha."
                            "d174ab68b8c3f11636a9c825d1153f6088bac79ca85c948bea"
                            "02f1134d0235d6.hash",
                            0},
                           {"000053.sst", 64000000},
                           {"000055.sha."
                            "d174ab68b8c3f11636a9c825d1153f6088bac79ca85c948bea"
                            "02f1134d0235d6.hash",
                            0},
                           {"000055.blob", 64000000},
                       });

  checksum::ChecksumHelper helper(directory);
  helper.checkMissingShaFiles();

  auto files = TRI_FilesDirectory(directory.c_str());
  std::sort(files.begin(), files.end());

  // no new files should have appeared, no files should have been deleted
  std::vector<std::string> expected = {
      {"000050.sha."
       "16f1e6b2f6b7cedcc122807551c5d04a0e339e4b406879006eccb580ef6d3545.hash"},
      {"000050.sst"},
      {"000051.sha."
       "908e00481d4913afdc583633624dd87176af80558cc3c2a8c5528ab89e9d5c60.hash"},
      {"000051.sst"},
      {"000053.sha."
       "d174ab68b8c3f11636a9c825d1153f6088bac79ca85c948bea02f1134d0235d6.hash"},
      {"000053.sst"},
      {"000055.blob"},
      {"000055.sha."
       "d174ab68b8c3f11636a9c825d1153f6088bac79ca85c948bea02f1134d0235d6.hash"},
      {"CURRENT"},
      {"MANIFEST-000005"},
      {"OPTIONS-000041"},
      {"OPTIONS-000043"},
  };

  ASSERT_EQ(expected, files);
}

TEST_F(RocksDBChecksumHelperTest, test_missing_sha_files) {
  FileMaker(directory, {
                           {"CURRENT", 16},
                           {"MANIFEST-000005", 10000},
                           {"OPTIONS-000041", 20000},
                           {"OPTIONS-000043", 20000},
                           {"000050.sst", 32000000},
                           {"000051.sha."
                            "908e00481d4913afdc583633624dd87176af80558cc3c2a8c5"
                            "528ab89e9d5c60.hash",
                            0},
                           {"000051.sst", 48000000},
                           {"000053.sst", 64000000},
                           {"000055.blob", 64000000},
                       });

  checksum::ChecksumHelper helper(directory);
  helper.checkMissingShaFiles();

  auto files = TRI_FilesDirectory(directory.c_str());
  std::sort(files.begin(), files.end());

  // 3 new sha files should have appeared
  std::vector<std::string> expected = {
      // new
      {"000050.sha."
       "b4bdf67d8d471ee4187a1132ce11aadf6681036e042199388b0105450ef9c71e.hash"},
      {"000050.sst"},
      {"000051.sha."
       "908e00481d4913afdc583633624dd87176af80558cc3c2a8c5528ab89e9d5c60.hash"},
      {"000051.sst"},
      // new
      {"000053.sha."
       "c33037ed23f5865dcaf4d31f3425f76d1b4598156cff437cbd726fe980617e46.hash"},
      {"000053.sst"},
      {"000055.blob"},
      // new
      {"000055.sha."
       "c33037ed23f5865dcaf4d31f3425f76d1b4598156cff437cbd726fe980617e46.hash"},
      {"CURRENT"},
      {"MANIFEST-000005"},
      {"OPTIONS-000041"},
      {"OPTIONS-000043"},
  };

  ASSERT_EQ(expected, files);
}

TEST_F(RocksDBChecksumHelperTest, test_superfluous_sha_files) {
  FileMaker(directory, {
                           {"CURRENT", 16},
                           {"MANIFEST-000005", 10000},
                           {"OPTIONS-000041", 20000},
                           {"OPTIONS-000043", 20000},
                           {"000050.sha."
                            "16f1e6b2f6b7cedcc122807551c5d04a0e339e4b406879006e"
                            "ccb580ef6d3545.hash",
                            0},
                           {"000051.sha."
                            "908e00481d4913afdc583633624dd87176af80558cc3c2a8c5"
                            "528ab89e9d5c60.hash",
                            0},
                           {"000051.sst", 48000000},
                           {"000053.sha."
                            "d174ab68b8c3f11636a9c825d1153f6088bac79ca85c948bea"
                            "02f1134d0235d6.hash",
                            0},
                           {"000055.sha."
                            "d174ab68b8c3f11636a9c825d1153f6088bac79ca85c948bea"
                            "02f1134d0235d6.hash",
                            0},
                       });

  checksum::ChecksumHelper helper(directory);
  helper.checkMissingShaFiles();

  auto files = TRI_FilesDirectory(directory.c_str());
  std::sort(files.begin(), files.end());

  // 3 sha files should have been deleted
  std::vector<std::string> expected = {
      // new
      {"000051.sha."
       "908e00481d4913afdc583633624dd87176af80558cc3c2a8c5528ab89e9d5c60.hash"},
      {"000051.sst"},
      {"CURRENT"},
      {"MANIFEST-000005"},
      {"OPTIONS-000041"},
      {"OPTIONS-000043"},
  };

  ASSERT_EQ(expected, files);
}
