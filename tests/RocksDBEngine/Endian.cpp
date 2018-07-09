////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Endian.h"
#include "RocksDBEngine/RocksDBFormat.h"

#include "catch.hpp"
#include <date/date.h>

using namespace arangodb;
using namespace arangodb::basics;


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBFormat functions class
TEST_CASE("RocksDBFormat", "[rocksdb][format]") {
  std::string out;
  
   SECTION("Little-Endian") {
     rocksutils::uintToPersistentLittleEndian<uint64_t>(out, 1);
     CHECK(out.size() == 8);
     CHECK(rocksutils::uintFromPersistentLittleEndian<uint64_t>(out.data()) == 1);
     out.clear();
     
     rocksutils::uintToPersistentLittleEndian<uint64_t>(out, 1337);
     CHECK(out.size() == 8);
     CHECK(rocksutils::uintFromPersistentLittleEndian<uint64_t>(out.data()) == 1337);
     out.clear();
     
     rocksutils::uintToPersistentLittleEndian<uint64_t>(out, 1212321);
     CHECK(out.size() == 8);
     CHECK(rocksutils::uintFromPersistentLittleEndian<uint64_t>(out.data()) == 1212321);
     out.clear();
     
     rocksutils::uintToPersistentLittleEndian<uint32_t>(out, 88888);
     CHECK(out.size() == 4);
     CHECK(rocksutils::uintFromPersistentLittleEndian<uint32_t>(out.data()) == 88888);
     out.clear();
   }
  
  SECTION("Big-Endian") {
    rocksutils::uintToPersistentBigEndian<uint64_t>(out, 1);
    CHECK(out.size() == 8);
    CHECK(rocksutils::uintFromPersistentBigEndian<uint64_t>(out.data()) == 1);
    out.clear();
    
    rocksutils::uintToPersistentBigEndian<uint64_t>(out, 1337);
    CHECK(out.size() == 8);
    CHECK(rocksutils::uintFromPersistentBigEndian<uint64_t>(out.data()) == 1337);
    out.clear();
    
    rocksutils::uintToPersistentBigEndian<uint64_t>(out, 1212321);
    CHECK(out.size() == 8);
    CHECK(rocksutils::uintFromPersistentBigEndian<uint64_t>(out.data()) == 1212321);
    out.clear();
    
    rocksutils::uintToPersistentBigEndian<uint32_t>(out, 88888);
    CHECK(out.size() == 4);
    CHECK(rocksutils::uintFromPersistentBigEndian<uint32_t>(out.data()) == 88888);
    out.clear();
  }
  
  SECTION("Specialized Little-Endian") {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);
    
    rocksutils::uint32ToPersistent(out, 1);
    CHECK(out.size() == 4);
    CHECK(rocksutils::uint32FromPersistent(out.data()) == 1);
    out.clear();
    
    rocksutils::uint16ToPersistent(out, 1337);
    CHECK(out.size() == 2);
    CHECK(rocksutils::uint16FromPersistent(out.data()) == 1337);
    out.clear();
    
    rocksutils::uint64ToPersistent(out, 1212321);
    CHECK(out.size() == 8);
    CHECK(rocksutils::uint64FromPersistent(out.data()) == 1212321);
    out.clear();
  }
  
  SECTION("Specialized Big-Endian") {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Big);
    
    rocksutils::uint32ToPersistent(out, 1);
    CHECK(out.size() == 4);
    CHECK(rocksutils::uint32FromPersistent(out.data()) == 1);
    out.clear();
    
    rocksutils::uint16ToPersistent(out, 1337);
    CHECK(out.size() == 2);
    CHECK(rocksutils::uint16FromPersistent(out.data()) == 1337);
    out.clear();
    
    rocksutils::uint64ToPersistent(out, 1212321);
    CHECK(out.size() == 8);
    CHECK(rocksutils::uint64FromPersistent(out.data()) == 1212321);
    out.clear();
  }
}
