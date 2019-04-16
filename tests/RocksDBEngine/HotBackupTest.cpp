////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Random/RandomGenerator.h"

#include "catch.hpp"

//#include "Basics/Exceptions.h"
//#include "Basics/VelocyPackHelper.h"
#include <velocypack/velocypack-aliases.h>
#include "RocksDBEngine/RocksDBHotBackup.h"

using namespace arangodb;
using namespace arangodb::basics;

static bool Initialized = false;
static uint64_t counter = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief Override base class to test functions
////////////////////////////////////////////////////////////////////////////////

class RocksDBHotBackupTest : public RocksDBHotBackup {
public:

  RocksDBHotBackupTest() = delete;
  RocksDBHotBackupTest(const VPackSlice body, VPackBuilder& report)
    : RocksDBHotBackup(body, report) {};

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
TEST_CASE("RocksDBHotBackup path tests", "[rocksdb][devel]") {

  VPackSlice config;
  VPackBuilder report;
  
  RocksDBHotBackupTest testee(config, report);

  SECTION("test_override") {
    CHECK(0 == testee.getPersistedId().compare("SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe"));
  }

  SECTION("test_date clean up") {
    CHECK(testee.buildDirectoryPath("2019-01-23T14:47:42Z","") ==
            "/var/db/hotbackups/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z");
  }

  SECTION("test_user string clean up") {
    CHECK(testee.buildDirectoryPath("2019-01-23T14:47:42Z","1\"2#3,14159") ==
            "/var/db/hotbackups/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z_1.2.3.14159");
    CHECK(testee.buildDirectoryPath("2019-01-23T14:47:42Z","Today\'s Hot Backup") ==
            "/var/db/hotbackups/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z_Today.s_Hot_Backup");
    std::string raw_string("Toodaay\'s hot");
    raw_string[1]=(char)1;
    raw_string[5]=(char)5;
    CHECK(testee.buildDirectoryPath("2019-01-23T14:47:42Z",raw_string) ==
            "/var/db/hotbackups/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z_Today.s_hot");
  }

  SECTION("test getRocksDBPath") {
    CHECK(testee.getDatabasePath() == "/var/db");
    CHECK(testee.getRocksDBPath() == "/var/db/engine-rocksdb");
  }
}


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBHotBackup create operation parameters
TEST_CASE("RocksDBHotBackup operation parameters", "[rocksdb][devel]") {

  SECTION("test_defaults") {
    const VPackSlice slice;
    VPackBuilder report;
    RocksDBHotBackupCreate testee(slice, report, true);
    CHECK(true == testee.isCreate());
    CHECK(testee.getTimestamp() == "");
    CHECK(10 == testee.getTimeout());
    CHECK(testee.getUserString() == "");
  }

  SECTION("test_simple") {
    VPackBuilder opBuilder;
    { VPackObjectBuilder a(&opBuilder);
      opBuilder.add("timeout", VPackValue(12345));
      opBuilder.add("id", VPackValue("2017-08-01T09:00:00Z"));
      opBuilder.add("label", VPackValue("first day"));
    }

    VPackBuilder report;
    RocksDBHotBackupCreate testee(opBuilder.slice(), report, false);
    testee.parseParameters();
    CHECK(testee.valid());
    CHECK(false == testee.isCreate());
    CHECK(12345 == testee.getTimeout());
    CHECK(testee.getDirectory() == "2017-08-01T09:00:00Z");
    CHECK(testee.getUserString() == "first day");
  }

  SECTION("test_timestamp_exception") {
    VPackBuilder opBuilder;
    { VPackObjectBuilder a(&opBuilder);
      opBuilder.add("timeout", VPackValue("12345"));
      opBuilder.add("timestamp", VPackValue("2017-08-01T09:00:00Z"));   // needed for exception
      opBuilder.add("label", VPackValue("makes timeoutMS throw")); //  to happen
    }

    VPackBuilder report;
    RocksDBHotBackupCreate testee(opBuilder.slice(), report, false);
    testee.parseParameters();
    CHECK(!testee.valid());
    CHECK((testee.resultSlice().isObject() && testee.resultSlice().hasKey("timeout")));
  }

}


////////////////////////////////////////////////////////////////////////////////
/// RocksDBHotBackupRestoreTest based upon CFilesSetup class from tests/Basic/files-test.cpp
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupRestoreTest : public RocksDBHotBackupRestore {
public:
  RocksDBHotBackupRestoreTest(VPackSlice const slice, VPackBuilder& report) :
    RocksDBHotBackupRestore(slice, report), _pauseRocksDBReturn(true),
    _restartRocksDBReturn(true), _holdTransactionsReturn(true) {
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

    _idRestore = "SNGL-9231534b-e1aa-4eb6-881a-0b6c798c6677_2019-02-15T20.51.13Z";
  }

  //RocksDBHotBackupRestoreTest(const VPackSlice body) : RocksDBHotBackup(body) {};

  std::string getDatabasePath() override {return _id;};

  std::string buildDirectoryPath(const std::string & time, const std::string & label) override
    {return RocksDBHotBackup::buildDirectoryPath(time, label);};

  std::string getPersistedId() override
    {return "SNGL-9231534b-e1aa-4eb6-881a-0b6c798c6677";};

  bool holdRocksDBTransactions() override {return _holdTransactionsReturn;};
  void releaseRocksDBTransactions() override {};


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
      CHECK(false == true);
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
      CHECK(false == true);
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
    CHECK(TRI_ERROR_NO_ERROR == retVal);

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
    pathname += "hotbackups";
    pathname += TRI_DIR_SEPARATOR_CHAR;
    pathname += _idRestore;
    retVal = TRI_CreateRecursiveDirectory(pathname.c_str(), systemError,
                                          systemErrorStr);

    CHECK(TRI_ERROR_NO_ERROR == retVal);

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



/// @brief test
TEST_CASE("RocksDBHotBackupRestore directories", "[rocksdb][devel]") {
  std::string restoringDir, tempname;
  bool retBool;

  SECTION("test createRestoringDirectory") {
    VPackBuilder report;
    RocksDBHotBackupRestoreTest testee(VPackSlice(), report);
    testee.createHotDirectory();

    retBool = testee.createRestoringDirectory(restoringDir);

    // spot check files in restoring dir
    CHECK( true == retBool );
    CHECK( TRI_ExistsFile(restoringDir.c_str()) );
    CHECK( TRI_IsDirectory(restoringDir.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "MANIFEST-000003";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "CURRENT";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sst";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link

    // verify still present in originating dir
    restoringDir = testee.rebuildPath(testee.getDirectoryRestore());
    CHECK( TRI_ExistsFile(restoringDir.c_str()) );
    CHECK( TRI_IsDirectory(restoringDir.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "MANIFEST-000003";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "CURRENT";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sst";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link

  }


  SECTION("test execute() normal directory path") {
    VPackBuilder report; 
    RocksDBHotBackupRestoreTest testee(VPackSlice(), report);
    testee.createDBDirectory();
    testee.createHotDirectory();

    testee.execute();

    CHECK( testee.success() );
#if 0
    // spot check files in restoring dir
    CHECK( true == retBool );
    CHECK( TRI_ExistsFile(restoringDir.c_str()) );
    CHECK( TRI_IsDirectory(restoringDir.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "MANIFEST-000003";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "CURRENT";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sst";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link

    // verify still present in originating dir
    restoringDir = testee.rebuildPath(testee.getDirectoryRestore());
    CHECK( TRI_ExistsFile(restoringDir.c_str()) );
    CHECK( TRI_IsDirectory(restoringDir.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "MANIFEST-000003";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "CURRENT";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) );
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sst";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
    tempname = restoringDir + TRI_DIR_SEPARATOR_CHAR + "000111.sha.e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.hash";
    CHECK( TRI_ExistsFile(tempname.c_str()) );
    CHECK( TRI_IsRegularFile(tempname.c_str()) ); // looks same as hard link
#endif
  }
}
