////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"

#include "date/date.h"

#include <string>
#include <chrono>

#include "Inspection/Transformers.h"
#include "Inspection/VPackWithErrorT.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

using namespace arangodb;
using namespace arangodb::velocypack;

struct TimeStampTransformerTest : public ::testing::Test {
  using TimeStamp = std::chrono::system_clock::time_point;
  inspection::TimeStampTransformer transformer{};
};

TEST_F(TimeStampTransformerTest, transforms_min) {
  auto target = std::string{};
  auto result = transformer.toSerialized(TimeStamp::min(), target);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(date::format("%FT%TZ", TimeStamp::min()), target);
}

TEST_F(TimeStampTransformerTest, transforms_now) {
  auto const& now = std::chrono::system_clock::now();

  auto target = std::string{};
  auto result = transformer.toSerialized(now, target);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(date::format("%FT%TZ", now), target);
}

TEST_F(TimeStampTransformerTest, transforms_back) {
  using namespace std::chrono;
  constexpr auto testinput = "2021-11-11T11:11:11Z";
  constexpr system_clock::time_point testoutput =
      std::chrono::sys_days{2021y / 11 / 11} + 11h + 11min + 11s;

  auto target = std::chrono::system_clock::time_point{};
  auto result = transformer.fromSerialized(testinput, target);

  EXPECT_EQ(target, testoutput);

  EXPECT_TRUE(result.ok());
}

TEST_F(TimeStampTransformerTest, parse_fails) {
  auto const& testinput = "2021-11-11 __:??:11";
  auto target = std::chrono::system_clock::time_point{};
  auto result = transformer.fromSerialized(testinput, target);

  ASSERT_EQ(result.error(),
            "failed to parse timestamp `2021-11-11 __:??:11` using format "
            "string `%FT%TZ`");
  EXPECT_FALSE(result.ok());
}

namespace {
struct IContainATimeStamp {
  std::chrono::system_clock::time_point stamp;

  auto operator==(IContainATimeStamp const& other) const {
    return stamp == other.stamp;
  }
};
template<class Inspector>
auto inspect(Inspector& f, IContainATimeStamp& x) {
  return f.object(x).fields(
      f.field("timeStamp", x.stamp)
          .transformWith(inspection::TimeStampTransformer{}));
}

}  // namespace

TEST_F(TimeStampTransformerTest, struct_with_timestamp_serializes) {
  using namespace std::chrono;
  auto input = IContainATimeStamp{
      .stamp = std::chrono::sys_days{2021y / 11 / 11} + 11h + 11min + 11s};

  auto res = inspection::serializeWithErrorT(input);

  ASSERT_TRUE(res.ok());
  ASSERT_EQ(res.get().toJson(),
            R"json({"timeStamp":"2021-11-11T11:11:11.000000000Z"})json");
}

TEST_F(TimeStampTransformerTest, struct_with_timestamp_deserializes) {
  using namespace std::chrono;

  auto input = R"({"timeStamp":"1900-01-01T11:11:11.000000000Z"})"_vpack;

  auto res = inspection::deserializeWithErrorT<IContainATimeStamp>(input);

  ASSERT_TRUE(res.ok());
  ASSERT_EQ(res.get(),
            IContainATimeStamp{.stamp = std::chrono::sys_days{1900y / 1 / 1} +
                                        11h + 11min + 11s});
}

TEST_F(TimeStampTransformerTest, transformer_is_left_inverse) {
  using namespace std::chrono;
  constexpr system_clock::time_point test =
      std::chrono::sys_days{2021y / 1 / 27} + 11h + 17min + 19s;

  auto deserialized = system_clock::time_point{};
  auto serialized = std::string{};

  auto serResult = transformer.toSerialized(test, serialized);

  EXPECT_TRUE(serResult.ok());
  EXPECT_EQ(date::format("2021-01-27T11:17:19.000000000Z", test), serialized);

  auto deserResult = transformer.fromSerialized(serialized, deserialized);

  EXPECT_TRUE(deserResult.ok());
  EXPECT_EQ(test, deserialized);
}
