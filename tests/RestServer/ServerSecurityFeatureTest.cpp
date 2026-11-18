////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "GeneralServer/ServerSecurityFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/ArangodServer.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

using namespace arangodb;

namespace {

/// @brief sets (or unsets) the FedRAMPHigh environment variable for the
/// lifetime of the object, and restores the previous state afterwards.
class ScopedFedRAMPHighEnv {
 public:
  explicit ScopedFedRAMPHighEnv(std::optional<std::string> const& value) {
    if (char const* old =
            std::getenv(ServerSecurityFeature::kFedRAMPHighEnvVar);
        old != nullptr) {
      _old = old;
    }
    set(value);
  }
  ~ScopedFedRAMPHighEnv() { set(_old); }

 private:
  static void set(std::optional<std::string> const& value) {
    if (value.has_value()) {
      ::setenv(ServerSecurityFeature::kFedRAMPHighEnvVar, value->c_str(), 1);
    } else {
      ::unsetenv(ServerSecurityFeature::kFedRAMPHighEnvVar);
    }
  }

  std::optional<std::string> _old;
};

bool detectFedRAMPHigh(std::optional<std::string> const& value) {
  ScopedFedRAMPHighEnv env(value);
  ArangodServer server(
      std::make_shared<options::ProgramOptions>("arangod", "", "", ""), "",
      "arangod", nullptr, nullptr, nullptr);
  server.addFeature<ServerSecurityFeature>();
  return ServerSecurityFeature::isFedRAMPHigh();
}

}  // namespace

TEST(ServerSecurityFeatureTest, fedramp_high_is_off_if_variable_is_unset) {
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
}

TEST(ServerSecurityFeatureTest, fedramp_high_is_off_if_variable_is_empty) {
  EXPECT_FALSE(detectFedRAMPHigh(""));
}

TEST(ServerSecurityFeatureTest, fedramp_high_is_on_if_variable_is_non_empty) {
  EXPECT_TRUE(detectFedRAMPHigh("1"));
  EXPECT_TRUE(detectFedRAMPHigh("false"));  // any non-empty value counts
  // flag is re-detected on each construction
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
}
