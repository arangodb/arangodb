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

#include "catch.hpp"

//#include "Basics/Exceptions.h"
//#include "Basics/VelocyPackHelper.h"
#include <velocypack/velocypack-aliases.h>
#include "RocksDBEngine/RocksDBHotBackup.h"

using namespace arangodb;

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
            "/var/db/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z");
  }

  SECTION("test_user string clean up") {
    CHECK(testee.buildDirectoryPath("2019-01-23T14:47:42Z","1\"2#3,14159") ==
            "/var/db/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z_1.2.3.14159");
    CHECK(testee.buildDirectoryPath("2019-01-23T14:47:42Z","Today\'s Hot Backup") ==
            "/var/db/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z_Today.s_Hot_Backup");
    std::string raw_string("Toodaay\'s hot");
    raw_string[1]=(char)1;
    raw_string[5]=(char)5;
    CHECK(testee.buildDirectoryPath("2019-01-23T14:47:42Z",raw_string) ==
            "/var/db/SNGL-d8e661e0-0202-48f3-801e-b6f36000aebe_2019-01-23T14.47.42Z_Today.s_hot");
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
    CHECK(10000 == testee.getTimeoutMS());
    CHECK(testee.getUserString() == "");
  }

  SECTION("test_simple") {
    VPackBuilder opBuilder;
    { VPackObjectBuilder a(&opBuilder);
      opBuilder.add("timeoutMS", VPackValue(12345));
      opBuilder.add("timestamp", VPackValue("2017-08-01T09:00:00Z"));
      opBuilder.add("userString", VPackValue("first day"));
    }

    RocksDBHotBackupCreate testee(opBuilder.slice());
    testee.parseParameters(rest::RequestType::DELETE_REQ);
    CHECK(testee.valid());
    CHECK(false == testee.isCreate());
    CHECK(12345 == testee.getTimeoutMS());
    CHECK(testee.getTimestamp() == "2017-08-01T09:00:00Z");
    CHECK(testee.getUserString() == "first day");
  }

  SECTION("test_timestamp_exception") {
    VPackBuilder opBuilder;
    { VPackObjectBuilder a(&opBuilder);
      opBuilder.add("timeoutMS", VPackValue("12345"));
      opBuilder.add("timestamp", VPackValue("2017-08-01T09:00:00Z"));   // needed for exception
      opBuilder.add("userString", VPackValue("makes timeoutMS throw")); //  to happen
    }

    RocksDBHotBackupCreate testee(opBuilder.slice());
    testee.parseParameters(rest::RequestType::DELETE_REQ);
    CHECK(!testee.valid());
    CHECK((testee.resultSlice().isObject() && testee.resultSlice().hasKey("timeoutMS")));
  }

}
