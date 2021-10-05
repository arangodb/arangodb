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
/// @author Ignacio Rodriguez
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "Rest/VstResponse.h"
#include "velocypack/Parser.h"
#include "velocypack/Dumper.h"
#include "Basics/encoding.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class VstResponseTest
    : public ::testing::Test {
};

auto const MAGIC_TYPE {0xf3};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

struct TestTypeHandler final : public arangodb::velocypack::CustomTypeHandler {
  void dump(arangodb::velocypack::Slice const& value, arangodb::velocypack::Dumper* dumper, arangodb::velocypack::Slice const& base) override final {
    throw std::runtime_error("Not implemented");
  }
  std::string toString(arangodb::velocypack::Slice const& value, arangodb::velocypack::Options const* options,
                       arangodb::velocypack::Slice const& base) override final {
    auto p {value.begin()};
    if (*p++ != MAGIC_TYPE) {
      throw std::runtime_error("This is not my type");
    }
    auto const v {arangodb::encoding::readNumber<uint64_t>(p, sizeof(uint64_t))};
    return std::to_string(v);
  }
};

TEST_F(VstResponseTest, add_payload_slice_json) {
  arangodb::VstResponse resp{arangodb::rest::ResponseCode::OK, 0};
  resp.setContentTypeRequested(arangodb::rest::ContentType::JSON);

  // create a builder
  arangodb::velocypack::Builder builder;

  builder.openObject();

  // use the builder to create the value I want
  uint8_t* p = builder.add(arangodb::StaticStrings::IdString,
                           arangodb::velocypack::ValuePair(9ULL, arangodb::velocypack::ValueType::Custom));

  *p++ = MAGIC_TYPE;
  arangodb::encoding::storeNumber<uint64_t>(p, 12345, sizeof(uint64_t));
  builder.close();

  auto const slice {builder.slice()};
  arangodb::velocypack::Options options;
  TestTypeHandler handler;
  options.customTypeHandler = &handler;
  resp.addPayload(slice, &options, true);
  auto const payload {resp.payload()};

  EXPECT_EQ(payload.length(), 15);
  EXPECT_EQ(payload.toString(), "{\"_id\":\"12345\"}");
}
