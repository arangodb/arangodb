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

#include "Mocks/Death_Test.h"
#include "Mocks/LogLevels.h"

#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/ArangodServer.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

using namespace arangodb;

namespace {

/// @brief sets (or unsets) the FEDRAMP_HIGH environment variable for the
/// lifetime of the object, and restores the previous state afterwards.
class ScopedFedRAMPHighEnv {
 public:
  explicit ScopedFedRAMPHighEnv(std::optional<std::string> const& value) {
    if (char const* old = std::getenv(ArangodServer::kFedRAMPHighEnvVar);
        old != nullptr) {
      _old = old;
    }
    set(value);
  }
  ~ScopedFedRAMPHighEnv() { set(_old); }

 private:
  static void set(std::optional<std::string> const& value) {
    if (value.has_value()) {
      ::setenv(ArangodServer::kFedRAMPHighEnvVar, value->c_str(), 1);
    } else {
      ::unsetenv(ArangodServer::kFedRAMPHighEnvVar);
    }
  }

  std::optional<std::string> _old;
};

// the ArangodServer constructor sets the global "always log security events"
// flag in FedRAMP High mode
using tests::ScopedAlwaysLogSecurityEvents;

std::unique_ptr<ArangodServer> makeServer() {
  return std::make_unique<ArangodServer>(
      std::make_shared<options::ProgramOptions>("arangod", "", "", ""), "",
      "arangod", nullptr, nullptr, nullptr);
}

bool detectFedRAMPHigh(std::optional<std::string> const& value) {
  ScopedFedRAMPHighEnv env(value);
  return makeServer()->isFedRAMPHigh();
}

}  // namespace

TEST(ArangodServerFedRAMPTest, fedramp_high_is_off_if_variable_is_unset) {
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
}

TEST(ArangodServerFedRAMPTest, fedramp_high_is_off_if_variable_is_off) {
  EXPECT_FALSE(detectFedRAMPHigh("off"));
}

TEST(ArangodServerFedRAMPTest, fedramp_high_is_on_if_variable_is_on) {
  ScopedAlwaysLogSecurityEvents guard;
  EXPECT_TRUE(detectFedRAMPHigh("on"));
  // flag is detected per server instance
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
  EXPECT_FALSE(detectFedRAMPHigh("off"));
}

// any value other than "on" or "off" (including the empty string) is
// rejected, and the
// server refuses to start. note: the FATAL log message goes to stdout in the
// test environment, so we only check that the process dies.
TEST(ArangodServerFedRAMPTest, fedramp_high_rejects_invalid_values) {
  for (char const* value : {"", "1", "false", "ON", " on"}) {
    EXPECT_DEATH_CORE_FREE(detectFedRAMPHigh(value), "")
        << "value: '" << value << "'";
  }
}

// in FedRAMP High mode, security events are logged right from the
// construction of the server, i.e. before options are processed.
TEST(ArangodServerFedRAMPTest, fedramp_high_forces_security_logging_early) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  EXPECT_TRUE(detectFedRAMPHigh("on"));
  EXPECT_TRUE(Logger::alwaysLogSecurityEvents());
}

TEST(ArangodServerFedRAMPTest, no_fedramp_high_keeps_security_logging) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
  EXPECT_FALSE(Logger::alwaysLogSecurityEvents());
}
