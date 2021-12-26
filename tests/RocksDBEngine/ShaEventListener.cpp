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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/Listeners/RocksDBShaCalculator.h"

#include <iostream>
#include <string>
#include "gtest/gtest.h"

using namespace arangodb::basics;

static bool Initialized = false;
static uint64_t counter = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// CFilesSetup class copied from tests/Basic/files-test.cpp
////////////////////////////////////////////////////////////////////////////////
struct CFilesSetup {
  CFilesSetup() : _directory(true) {
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

  ~CFilesSetup() {
    // let's be sure we delete the right stuff
    TRI_ASSERT(_directory.length() > 10);

    TRI_RemoveDirectory(_directory.c_str());
  }

  void writeFile(char const* blob) {
    StringBuffer filename(true);
    filename.appendText(_directory);
    filename.appendChar(TRI_DIR_SEPARATOR_CHAR);
    filename.appendText("tmp-");
    filename.appendInteger(++counter);
    filename.appendInteger(arangodb::RandomGenerator::interval(UINT32_MAX));

    FILE* fd = fopen(filename.c_str(), "wb");

    if (fd) {
      size_t numWritten = fwrite(blob, strlen(blob), 1, fd);
      (void)numWritten;
      fclose(fd);
    } else {
      EXPECT_TRUE(false);
    }
  }

  void writeFile(char const* name, char const* blob) {
    StringBuffer filename(true);
    filename.appendText(_directory);
    filename.appendChar(TRI_DIR_SEPARATOR_CHAR);
    filename.appendText(name);

    FILE* fd = fopen(filename.c_str(), "wb");

    if (fd) {
      size_t numWritten = fwrite(blob, strlen(blob), 1, fd);
      (void)numWritten;
      fclose(fd);
    } else {
      EXPECT_TRUE(false);
    }
  }

  StringBuffer _directory;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        action tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test actions
////////////////////////////////////////////////////////////////////////////////

TEST(RocksDBShaCalculatorThread, sha_a_new_file) {
  CFilesSetup s;

  std::string new_sst = s._directory.c_str();
  new_sst += TRI_DIR_SEPARATOR_CHAR;
  new_sst += "000042.sst";

  auto ret_val = TRI_WriteFile(new_sst.c_str(), "the quick brown fox", 19);
  EXPECT_EQ(ret_val, TRI_ERROR_NO_ERROR);

  auto [good, hash] = arangodb::RocksDBShaCalculatorThread::shaCalcFile(new_sst.c_str());
  EXPECT_TRUE(good);
  EXPECT_EQ(hash,
            "9ecb36561341d18eb65484e833efea61edc74b84cf5e6ae1b81c63533e25fc8f");
}

TEST(RocksDBShaCalculatorThread, sha_a_different_new_file) {
  CFilesSetup s;

  std::string new_sst = s._directory.c_str();
  new_sst += TRI_DIR_SEPARATOR_CHAR;
  new_sst += "000042.sst";

  auto ret_val = TRI_WriteFile(new_sst.c_str(), "12345 67890 12345 67890", 23);
  EXPECT_EQ(ret_val, TRI_ERROR_NO_ERROR);

  auto [good, hash] = arangodb::RocksDBShaCalculatorThread::shaCalcFile(new_sst.c_str());
  EXPECT_TRUE(good);
  EXPECT_EQ(hash,
            "e7f5561536b5891e35d6021015d67d5798b3731088b44dcebf6bad03785ac8c2");
}

TEST(RocksDBShaCalculatorThread, sha_a_non_existing_file) {
  CFilesSetup s;

  std::string sst = s._directory.c_str();
  sst += TRI_DIR_SEPARATOR_CHAR;
  sst += "does-not-exist";

  auto [good, hash] = arangodb::RocksDBShaCalculatorThread::shaCalcFile(sst.c_str());
  EXPECT_FALSE(good);
  EXPECT_EQ(hash, "");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test checkMissingShaFiles scenarios
////////////////////////////////////////////////////////////////////////////////
class TestRocksDBShaCalculatorThread : public arangodb::RocksDBShaCalculatorThread {
 public:
  TestRocksDBShaCalculatorThread(arangodb::application_features::ApplicationServer& server)
      : arangodb::RocksDBShaCalculatorThread(server, "testListener") {
    // sample sha values are simulated, not real
    setup.writeFile("MANIFEST-000004", "some manifest data");
    setup.writeFile("CURRENT", "MANIFEST-000004\n");
    setup.writeFile("IDENTITY", "no idea what goes here");
    setup.writeFile("037793.sst", "raw data 1");
    setup.writeFile(
        "037793.sha."
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash",
        "");
    setup.writeFile("037684.sst", "raw data 2");
    setup.writeFile(
        "086218.sha."
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash",
        "");
    setup.writeFile("086219.sst", "raw data 3");
  };

  CFilesSetup setup;

  virtual std::string getRocksDBPath() override {
    return setup._directory.c_str();
  }

  std::string pathName(const char* name) {
    std::string retpath;
    retpath = setup._directory.c_str();
    retpath += TRI_DIR_SEPARATOR_CHAR;
    retpath += name;
    return retpath;
  };
};

TEST(CheckMissingShaFilesSimple, verify_common_situations) {
  arangodb::application_features::ApplicationServer server{nullptr, nullptr};
  TestRocksDBShaCalculatorThread tr{server};

  tr.checkMissingShaFiles(tr.setup._directory.c_str(), 0);

  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("MANIFEST-000004").c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("CURRENT").c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("IDENTITY").c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("037793.sst").c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("037793.sha."
                                         "e3b0c44298fc1c149afbf4c8996fb92427ae4"
                                         "1e4649b934ca495991b7852b855.hash")
                                 .c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("037684.sst").c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("037684.sha."
                                         "2db3c4a7da801356e4efda0d65229d0baadf6"
                                         "950b366418e96abb7ece9c56c12.hash")
                                 .c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("086219.sst").c_str()));
  EXPECT_TRUE(TRI_ExistsFile(tr.pathName("086219.sha."
                                         "5d3cfa346c3852c0c108d720d580cf9991074"
                                         "9f17c8429c07c1c2d714be2b7ff.hash")
                                 .c_str()));

  EXPECT_TRUE(!TRI_ExistsFile(tr.pathName("086218.sha."
                                          "e3b0c44298fc1c149afbf4c8996fb92427ae"
                                          "41e4649b934ca495991b7852b855.hash")
                                  .c_str()));
}
