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

#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"

#include "gtest/gtest.h"

#include "Mocks/Servers.h"

//#include "Basics/Exceptions.h"
//#include "Basics/VelocyPackHelper.h"
#include <velocypack/velocypack-aliases.h>

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBHotBackup.h"
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif

#include "Rest/Version.h"

using namespace arangodb;
using namespace arangodb::basics;

#ifdef USE_ENTERPRISE

static bool Initialized = false;
static uint64_t counter = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Override base class to test functions
////////////////////////////////////////////////////////////////////////////////

class RocksDBHotBackupTest : public RocksDBHotBackup {
public:
  RocksDBHotBackupTest() = delete;
  RocksDBHotBackupTest(HotBackupFeature& feature, const VPackSlice body, VPackBuilder& report)
      : RocksDBHotBackup(feature, body, report){};

  std::string getDatabasePath() override {return "/var/db";};

  std::string buildDirectoryPath(const std::string & time, const std::string & userString) override
    {return RocksDBHotBackup::buildDirectoryPath(time, userString);};

  std::string getPersistedId() override
    {return "SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe";};

};// class RocksDBHotBackupTest

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBHotBackup::buildDirectoryPath
class RocksDBHotBackupPathTests : public ::testing::Test {
protected:
  VPackSlice config;
  VPackBuilder report;
  tests::mocks::MockAqlServer server;
  HotBackupFeature& feature;
  RocksDBHotBackupTest testee;

  RocksDBHotBackupPathTests()
      : server(),
        feature(server.getFeature<HotBackupFeature>()),
        testee(feature, config, report) {}
};


TEST_F(RocksDBHotBackupPathTests, test_override) {
  EXPECT_EQ(testee.getPersistedId().compare("SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe"), 0);
}

#ifdef _WIN32
TEST_F(RocksDBHotBackupPathTests, test_date_clean_up) {
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z",""),
          "/var/db\\backups\\2019-01-23T14.47.42Z");
}

TEST_F(RocksDBHotBackupPathTests, test_user_string_clean_up) {
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z","1\"2#3,14159"),
            "/var/db\\backups\\2019-01-23T14.47.42Z_1.2.3.14159");
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z","Today\'s Hot Backup"),
            "/var/db\\backups\\2019-01-23T14.47.42Z_Today.s_Hot_Backup");
  std::string raw_string("Toodaay\'s hot");
  raw_string[1]=(char)1;
  raw_string[5]=(char)5;
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z",raw_string),
            "/var/db\\backups\\2019-01-23T14.47.42Z_Today.s_hot");
}

TEST_F(RocksDBHotBackupPathTests, test_getRocksDBPath) {
  EXPECT_EQ(testee.getDatabasePath(), "/var/db");
  EXPECT_EQ(testee.getRocksDBPath(), "/var/db\\engine-rocksdb");
}
#else
TEST_F(RocksDBHotBackupPathTests, test_date_clean_up) {
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z",""),
          "/var/db/backups/2019-01-23T14.47.42Z");
}

TEST_F(RocksDBHotBackupPathTests, test_user_string_clean_up) {
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z","1\"2#3,14159"),
            "/var/db/backups/2019-01-23T14.47.42Z_1.2.3.14159");
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z","Today\'s Hot Backup"),
            "/var/db/backups/2019-01-23T14.47.42Z_Today.s_Hot_Backup");
  std::string raw_string("Toodaay\'s hot");
  raw_string[1]=(char)1;
  raw_string[5]=(char)5;
  EXPECT_EQ(testee.buildDirectoryPath("2019-01-23T14:47:42Z",raw_string),
            "/var/db/backups/2019-01-23T14.47.42Z_Today.s_hot");
}

TEST_F(RocksDBHotBackupPathTests, test_getRocksDBPath) {
  EXPECT_EQ(testee.getDatabasePath(), "/var/db");
  EXPECT_EQ(testee.getRocksDBPath(), "/var/db/engine-rocksdb");
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBHotBackup create operation parameters
TEST(RocksDBHotBackupOperationParameters, test_defaults) {
  const VPackSlice slice;
  VPackBuilder report;
  tests::mocks::MockAqlServer server;
  HotBackupFeature& feature = server.getFeature<HotBackupFeature>();
  RocksDBHotBackupCreate testee(feature, slice, report, true);

  EXPECT_TRUE(testee.isCreate());
  EXPECT_EQ(testee.getTimestamp(), "");
  EXPECT_EQ(testee.getTimeout(), 10.0);
  EXPECT_EQ(testee.getUserString(), "");
}

TEST(RocksDBHotBackupOperationParameters, test_simple) {
  VPackBuilder opBuilder;
  { VPackObjectBuilder a(&opBuilder);
    opBuilder.add("timeout", VPackValue(12345));
    opBuilder.add("id", VPackValue("2017-08-01T09:00:00Z"));
    opBuilder.add("label", VPackValue("first day"));
  }

  VPackBuilder report;
  tests::mocks::MockAqlServer server;
  HotBackupFeature& feature = server.getFeature<HotBackupFeature>();
  RocksDBHotBackupCreate testee(feature, opBuilder.slice(), report, false);
  testee.parseParameters();

  EXPECT_TRUE(testee.valid());
  EXPECT_FALSE(testee.isCreate());
  EXPECT_EQ(testee.getTimeout(), 12345.0);
  EXPECT_EQ(testee.getDirectory(), "2017-08-01T09:00:00Z");
  EXPECT_EQ(testee.getUserString(), "first day");
}

TEST(RocksDBHotBackupOperationParameters, test_timestamp_exception) {
  VPackBuilder opBuilder;
  { VPackObjectBuilder a(&opBuilder);
    opBuilder.add("timeout", VPackValue("12345"));
    opBuilder.add("timestamp", VPackValue("2017-08-01T09:00:00Z"));   // needed for exception
    opBuilder.add("label", VPackValue("makes timeoutMS throw")); //  to happen
  }

  VPackBuilder report;
  tests::mocks::MockAqlServer server;
  HotBackupFeature& feature = server.getFeature<HotBackupFeature>();
  RocksDBHotBackupCreate testee(feature, opBuilder.slice(), report, false);
  testee.parseParameters();

  EXPECT_FALSE(testee.valid());
  EXPECT_TRUE(testee.resultSlice().isObject() && testee.resultSlice().hasKey("timeout"));
}


////////////////////////////////////////////////////////////////////////////////
/// RocksDBHotBackupRestoreTest based upon CFilesSetup class from tests/Basic/files-test.cpp
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupRestoreTest : public RocksDBHotBackupRestore {
public:
 RocksDBHotBackupRestoreTest(HotBackupFeature& feature, VPackSlice const slice,
                             VPackBuilder& report)
     : RocksDBHotBackupRestore(feature, slice, report),
       _pauseRocksDBReturn(true),
       _restartRocksDBReturn(true),
       _holdTransactionsReturn(true) {
   long systemError;
   std::string errorMessage;

   if (!Initialized) {
     Initialized = true;
     arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
   }


   _id = TRI_GetTempPath();
   _id += TRI_DIR_SEPARATOR_CHAR;
   _id += "arangotest-";
   _id += std::to_string(TRI_microtime());
   _id += std::to_string(arangodb::RandomGenerator::interval(UINT32_MAX));

   TRI_CreateDirectory(_id.c_str(), systemError, errorMessage);

   _idRestore =
       "SNGL-9231534b-e1aa-4eb6-881a-0b6c798c6677_2019-02-15T20.51.13Z";
  }

  //RocksDBHotBackupRestoreTest(const VPackSlice body) : RocksDBHotBackup(body) {};

  std::string getDatabasePath() override {return _id;};

  std::string buildDirectoryPath(const std::string & time, const std::string & label) override
    {return RocksDBHotBackup::buildDirectoryPath(time, label);};

  std::string getPersistedId() override
    {return "SNGL-9231534b-e1aa-4eb6-881a-0b6c798c6677";};

  bool holdRocksDBTransactions() override {return _holdTransactionsReturn;};
  void releaseRocksDBTransactions() override {};

  virtual bool performViewRemoval() const override {
    return false;
  }

  ~RocksDBHotBackupRestoreTest () {
    // let's be sure we delete the right stuff
    TRI_ASSERT(_id.length() > 10);

    TRI_RemoveDirectory(_id.c_str());
  }

  StringBuffer* writeFile (const char* blob) {
    StringBuffer* filename = new StringBuffer(true);
    filename->appendText(_id);
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
      EXPECT_TRUE(false);
    }

    return filename;
  }

  void writeFile(const char * pathname, const char * filename, const char * blob) {
    std::string filepath;

    filepath = pathname;
    filepath += TRI_DIR_SEPARATOR_CHAR;
    filepath += filename;

    FILE* fd = fopen(filepath.c_str(), "wb");

    if (fd) {
      size_t numWritten = fwrite(blob, strlen(blob), 1, fd);
      (void) numWritten;
      fclose(fd);
    }
    else {
      EXPECT_TRUE(false);
    }

    return;
  }


  /// @brief Create an engine-rocksdb directory with a few files
  void createDBDirectory() {
    std::string pathname, systemErrorStr;
    int retVal;
    long systemError;

    pathname = getRocksDBPath();
    retVal = TRI_CreateRecursiveDirectory(pathname.c_str(), systemError,
                                          systemErrorStr);
    EXPECT_EQ(TRI_ERROR_NO_ERROR, retVal);

    writeFile(pathname.c_str(), "MANIFEST-000007", "manifest info");
    writeFile(pathname.c_str(), "CURRENT", "MANIFEST-000007\n");
    writeFile(pathname.c_str(), "IDENTITY", "huh?");
    writeFile(pathname.c_str(), "000221.sst", "raw data 1");
    writeFile(pathname.c_str(), "000221.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash", "");
    writeFile(pathname.c_str(), "001442.sst", "raw data 2");
    writeFile(pathname.c_str(), "001442.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash", "");
    writeFile(pathname.c_str(), "001447.sst", "raw data 3");
    writeFile(pathname.c_str(), "001447.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash", "");

  } // createDBDirectory

  void createHotDirectory() {
    std::string pathname, systemErrorStr;
    int retVal;
    long systemError;

    pathname = getDatabasePath();
    pathname += TRI_DIR_SEPARATOR_CHAR;
    pathname += "backups";
    pathname += TRI_DIR_SEPARATOR_CHAR;
    pathname += _idRestore;
    pathname += TRI_DIR_SEPARATOR_CHAR;
    pathname += "engine_rocksdb";
    retVal = TRI_CreateRecursiveDirectory(pathname.c_str(), systemError,
                                          systemErrorStr);

    EXPECT_EQ(TRI_ERROR_NO_ERROR, retVal);

    writeFile(pathname.c_str(), "../META", "{\"version\":\"" ARANGODB_VERSION "\", \"datetime\":\"xxx\", \"id\":\"xxx\"}");
    writeFile(pathname.c_str(), "MANIFEST-000003", "manifest info");
    writeFile(pathname.c_str(), "CURRENT", "MANIFEST-000003\n");
    writeFile(pathname.c_str(), "IDENTITY", "huh?");
    writeFile(pathname.c_str(), "000111.sst", "raw data 1");
    writeFile(pathname.c_str(), "000111.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash", "");
    writeFile(pathname.c_str(), "000223.sst", "raw data 2");
    writeFile(pathname.c_str(), "000223.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash", "");
    writeFile(pathname.c_str(), "000333.sst", "raw data 3");
    writeFile(pathname.c_str(), "000333.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash", "");
  } // createHotDirectory

  void startGlobalShutdown() override {};

  std::string _id;
  bool _pauseRocksDBReturn;
  bool _restartRocksDBReturn;
  bool _holdTransactionsReturn;
};// class RocksDBHotBackupRestoreTest


// Deactivated on Windows for now.

#ifndef _WIN32

/// @brief test
TEST(RocksDBHotBackupRestoreDirectories, test_createRestoringDirectory) {
  std::string fullRestoringDir, restoringDir, restoringSearchDir, tempname;
  bool retBool;

  VPackBuilder report;
  tests::mocks::MockAqlServer server;
  HotBackupFeature& feature = server.getFeature<HotBackupFeature>();
  RocksDBHotBackupRestoreTest testee(feature, VPackSlice(), report);
  testee.createHotDirectory();

  retBool = testee.createRestoringDirectories(fullRestoringDir, restoringDir, restoringSearchDir);

  // spot check files in restoring dir
  EXPECT_TRUE( retBool );
  EXPECT_TRUE( TRI_ExistsFile(restoringDir.c_str()) );
  EXPECT_TRUE( TRI_IsDirectory(restoringDir.c_str()) );
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "MANIFEST-000003";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) );
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "CURRENT";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) );
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sst";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link

  // verify still present in originating dir
  restoringDir = testee.rebuildPath(testee.getDirectoryRestore() +
                                    TRI_DIR_SEPARATOR_CHAR + "engine_rocksdb");
  EXPECT_TRUE( TRI_ExistsFile(restoringDir.c_str()) );
  EXPECT_TRUE( TRI_IsDirectory(restoringDir.c_str()) );
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "MANIFEST-000003";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) );
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "CURRENT";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) );
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sst";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
  tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash";
  EXPECT_TRUE( TRI_ExistsFile(tempname.c_str()) );
  EXPECT_TRUE( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
}


TEST(RocksDBHotBackupRestoreTest, test_execute_normal_directory_path) {
  VPackBuilder report;
  tests::mocks::MockAqlServer server;
  HotBackupFeature& feature = server.getFeature<HotBackupFeature>();
  RocksDBHotBackupRestoreTest testee(feature, VPackSlice(), report);

  testee.createDBDirectory();
  testee.createHotDirectory();

  testee.execute();

  EXPECT_TRUE( testee.success() );
}
#endif
#endif
