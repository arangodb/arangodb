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

  RocksDBHotBackupTest() : RocksDBHotBackup(VPackSlice()) {};
  RocksDBHotBackupTest(const VPackSlice body) : RocksDBHotBackup(body) {};

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
  RocksDBHotBackupTest testee;

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
    RocksDBHotBackupCreate testee(slice);
    CHECK(true == testee.isCreate());
    CHECK(testee.getTimestamp() == "");
    CHECK(10 == testee.getTimeout());
    CHECK(testee.getUserString() == "");
  }

  SECTION("test_simple") {
    VPackBuilder opBuilder;
    { VPackObjectBuilder a(&opBuilder);
      opBuilder.add("timeout", VPackValue(12345));
      opBuilder.add("timestamp", VPackValue("2017-08-01T09:00:00Z"));
      opBuilder.add("userString", VPackValue("first day"));
    }

    RocksDBHotBackupCreate testee(opBuilder.slice());
    testee.parseParameters(rest::RequestType::DELETE_REQ);
    CHECK(testee.valid());
    CHECK(false == testee.isCreate());
    CHECK(12345 == testee.getTimeout());
    CHECK(testee.getTimestamp() == "2017-08-01T09:00:00Z");
    CHECK(testee.getUserString() == "first day");
  }

  SECTION("test_timestamp_exception") {
    VPackBuilder opBuilder;
    { VPackObjectBuilder a(&opBuilder);
      opBuilder.add("timeout", VPackValue("12345"));
      opBuilder.add("timestamp", VPackValue("2017-08-01T09:00:00Z"));   // needed for exception
      opBuilder.add("userString", VPackValue("makes timeoutMS throw")); //  to happen
    }

    RocksDBHotBackupCreate testee(opBuilder.slice());
    testee.parseParameters(rest::RequestType::DELETE_REQ);
    CHECK(!testee.valid());
    CHECK((testee.resultSlice().isObject() && testee.resultSlice().hasKey("timeout")));
  }

}


////////////////////////////////////////////////////////////////////////////////
/// RocksDBHotBackupRestoreTest based upon CFilesSetup class from tests/Basic/files-test.cpp
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupRestoreTest : public RocksDBHotBackupRestore {
public:
  RocksDBHotBackupRestoreTest() : RocksDBHotBackupRestore(VPackSlice()) {
    long systemError;
    std::string errorMessage;

    if (!Initialized) {
      Initialized = true;
      arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
    }

    _directory = TRI_GetTempPath();
    _directory += TRI_DIR_SEPARATOR_CHAR;
    _directory += "arangotest-";
    _directory += static_cast<uint64_t>(TRI_microtime());
    _directory += arangodb::RandomGenerator::interval(UINT32_MAX);

    TRI_CreateDirectory(_directory.c_str(), systemError, errorMessage);
  }

  //RocksDBHotBackupRestoreTest(const VPackSlice body) : RocksDBHotBackup(body) {};

  std::string getDatabasePath() override {return _directory;};

  std::string buildDirectoryPath(const std::string & time, const std::string & userString) override
    {return RocksDBHotBackup::buildDirectoryPath(time, userString);};

  std::string getPersistedId() override
    {return "SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe";};

  bool pauseRocksDB() override {return _pauseRocksDBReturn;};
  bool restartRocksDB() override {return _restartRocksDBReturn;};
  bool holdRocksDBTransactions() override {return _holdTransactionsReturn;};
  void releaseRocksDBTransactions() override {};


  ~RocksDBHotBackupRestoreTest () {
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

  StringBuffer * writeFile (const char * name, const char * blob) {
    StringBuffer* filename = new StringBuffer(true);
    filename->appendText(_directory);
    filename->appendChar(TRI_DIR_SEPARATOR_CHAR);
    filename->appendText(name);

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

  std::string _directory;
  bool _pauseRocksDBReturn;
  bool _restartRocksDBReturn;
  bool _holdTransactionsReturn;
};



#if 0
/// @brief test
TEST_CASE("RocksDBHotBackup path tests", "[rocksdb][devel]") {
  RocksDBHotBackupTest testee;

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

}
#endif
