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
#include "Logger/LoggerOptions.h"
#include "Logger/LoggerOptionsProvider.h"
#include "ProgramOptions/ArgumentParser.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/ArangodServer.h"
#include "RestServer/FedRAMPHigh.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace arangodb;

namespace {

/// @brief sets (or unsets) the FEDRAMP_HIGH environment variable for the
/// lifetime of the object, and restores the previous state afterwards.
class ScopedFedRAMPHighEnv {
 public:
  explicit ScopedFedRAMPHighEnv(std::optional<std::string> const& value) {
    if (char const* old = std::getenv(kFedRAMPHighEnvVar); old != nullptr) {
      _old = old;
    }
    set(value);
  }
  ~ScopedFedRAMPHighEnv() { set(_old); }

 private:
  static void set(std::optional<std::string> const& value) {
    if (value.has_value()) {
      ::setenv(kFedRAMPHighEnvVar, value->c_str(), 1);
    } else {
      ::unsetenv(kFedRAMPHighEnvVar);
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

/// @brief parses `args` as command line with the real logger option
/// declarations. if `fedRAMPHigh` is set, the FedRAMP High defaults are applied
/// before the options are declared, as ArangodServer does.
LoggerOptions parseLoggerOptions(bool fedRAMPHigh,
                                 std::vector<std::string> args) {
  LoggerOptionsProvider provider;
  if (fedRAMPHigh) {
    applyFedRAMPHighLoggerDefaults(provider.mutableOptions());
  }
  auto programOptions =
      std::make_shared<options::ProgramOptions>("arangod", "", "", "");
  provider.declareOptions(programOptions);

  args.insert(args.begin(), "arangod");
  std::vector<char*> argv;
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  EXPECT_TRUE(options::ArgumentParser(programOptions.get())
                  .parse(static_cast<int>(argv.size()), argv.data()));
  return provider.options();
}

}  // namespace

TEST(FedRAMPHighSettingTest, unset_variable_means_off) {
  EXPECT_EQ(parseFedRAMPHighSetting(std::nullopt), FedRAMPHighSetting::kOff);
}

TEST(FedRAMPHighSettingTest, off_means_off) {
  EXPECT_EQ(parseFedRAMPHighSetting("off"), FedRAMPHighSetting::kOff);
}

TEST(FedRAMPHighSettingTest, on_means_on) {
  EXPECT_EQ(parseFedRAMPHighSetting("on"), FedRAMPHighSetting::kOn);
}

// a mistyped value must never silently select a mode, so everything except
// the exact spellings "on" and "off" is invalid.
TEST(FedRAMPHighSettingTest, any_other_value_is_invalid) {
  for (std::string_view value :
       {"", "1", "0", "true", "false", "ON", "Off", " on", "on ", "onn"}) {
    EXPECT_EQ(parseFedRAMPHighSetting(value), FedRAMPHighSetting::kInvalid)
        << "value: '" << value << "'";
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                     FedRAMP High logger options
// -----------------------------------------------------------------------------

TEST(FedRAMPHighLoggerOptionsTest,
     security_messages_are_not_forced_by_default) {
  EXPECT_FALSE(
      parseLoggerOptions(/*fedRAMPHigh*/ false, {}).alwaysLogSecurityMessages);
}

TEST(FedRAMPHighLoggerOptionsTest, security_messages_are_forced_by_default) {
  auto options = parseLoggerOptions(/*fedRAMPHigh*/ true, {});
  EXPECT_TRUE(options.alwaysLogSecurityMessages);
  EXPECT_TRUE(checkFedRAMPHighLoggerOptions(options).ok());
}

TEST(FedRAMPHighLoggerOptionsTest, explicitly_forcing_security_messages_is_ok) {
  auto options = parseLoggerOptions(
      /*fedRAMPHigh*/ true, {"--log.always-log-security-messages", "true"});
  EXPECT_TRUE(options.alwaysLogSecurityMessages);
  EXPECT_TRUE(checkFedRAMPHighLoggerOptions(options).ok());
}

TEST(FedRAMPHighLoggerOptionsTest, disabling_forced_security_messages_fails) {
  auto options = parseLoggerOptions(
      /*fedRAMPHigh*/ true, {"--log.always-log-security-messages", "false"});
  auto result = checkFedRAMPHighLoggerOptions(options);
  ASSERT_TRUE(result.fail());
  EXPECT_NE(result.errorMessage().find("--log.always-log-security-messages"),
            std::string::npos)
      << result.errorMessage();
}

// -----------------------------------------------------------------------------
// --SECTION--                                  FedRAMP High mode of the server
// -----------------------------------------------------------------------------

TEST(ArangodServerFedRAMPTest, fedramp_high_is_off_if_variable_is_unset) {
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
}

TEST(ArangodServerFedRAMPTest, fedramp_high_is_on_if_variable_is_on) {
  ScopedAlwaysLogSecurityEvents guard;
  EXPECT_TRUE(detectFedRAMPHigh("on"));
}

TEST(ArangodServerFedRAMPTest, fedramp_high_is_detected_per_server_instance) {
  ScopedAlwaysLogSecurityEvents guard;
  EXPECT_TRUE(detectFedRAMPHigh("on"));
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
}

// which values are invalid is covered by FedRAMPHighSettingTest. here we only
// check that the server refuses to start on an invalid value. note: the FATAL
// log message goes to stdout in the test environment, so we can only check
// that the process dies.
TEST(ArangodServerFedRAMPTest, invalid_value_aborts_startup) {
  EXPECT_DEATH_CORE_FREE(detectFedRAMPHigh("ON"), "");
}

// in FedRAMP High mode, security events are logged right from the
// construction of the server, i.e. before options are processed.
TEST(ArangodServerFedRAMPTest, fedramp_high_forces_security_logging_early) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  EXPECT_TRUE(detectFedRAMPHigh("on"));
  EXPECT_TRUE(Logger::alwaysLogSecurityEvents());
}

TEST(ArangodServerFedRAMPTest,
     no_fedramp_high_does_not_force_security_logging) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  EXPECT_FALSE(detectFedRAMPHigh(std::nullopt));
  EXPECT_FALSE(Logger::alwaysLogSecurityEvents());
}
