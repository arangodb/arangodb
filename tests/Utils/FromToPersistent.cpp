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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "RocksDBEngine/RocksDBCommon.h"

#include <memory>

TEST_CASE("FromToPersistentUint64", "[serialization]") {
  auto out1 = std::unique_ptr<char>(new char[sizeof(uint64_t)]);
  auto out2 = std::unique_ptr<char>(new char[sizeof(uint64_t)]);
  char* out1p = out1.get();
  char* out2p = out2.get();

  arangodb::rocksutils::toPersistent<uint64_t>(1337, out1p);
  out1p -= sizeof(uint64_t); //moves pointer forward
  arangodb::rocksutils::uintToPersistent<uint64_t>(out2p, 1337);

  // make sure we get the same as we put in
  REQUIRE(arangodb::rocksutils::fromPersistent<uint64_t>(out1p) == 1337);
  out1p -= sizeof(uint64_t); //moves pointer forward
  REQUIRE(arangodb::rocksutils::uintFromPersistent<uint64_t>(out2p) == 1337);

  REQUIRE(std::memcmp(out1p,out2p,sizeof(uint64_t)) == 0);
}
