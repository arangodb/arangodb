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
#include <string>
#include <string_view>
#include <vector>

using namespace arangodb;

namespace {

// the ArangodServer constructor sets the global "always log security events"
// flag in FedRAMP High mode
using tests::ScopedAlwaysLogSecurityEvents;

/// @brief exposes the startup steps of ArangodServer that run before any
/// features are created, so that tests can drive them like server::run() does.
class TestArangodServer final : public ArangodServer {
 public:
  using ArangodServer::ArangodServer;
  using ArangodServer::getOptions;

  void declareParseAndProcessOptions(std::vector<std::string> args) {
    declareOptions();
    // without this, a missing configuration file aborts the startup
    args.insert(args.begin(), {"arangod", "--configuration", "none"});
    std::vector<char*> argv;
    for (auto& arg : args) {
      argv.push_back(arg.data());
    }
    parseOptions(static_cast<int>(argv.size()), argv.data());
    processOptions();
  }
};

std::unique_ptr<TestArangodServer> makeServer(bool fedRAMPHigh) {
  return std::make_unique<TestArangodServer>(
      std::make_shared<options::ProgramOptions>("arangod", "", "", ""), "",
      "arangod", nullptr, nullptr, nullptr, fedRAMPHigh);
}

/// @brief puts the logger into the state it has during the early server
/// startup, i.e. inactive. some logger settings can only be changed then, and
/// an inactive logger writes to stderr, which is what death tests can match
/// (the active test logger writes to stdout). to be called in the child process
/// of a death test only.
void deactivateLogger() { Logger::shutdown(); }

/// @brief runs the option handling of the server startup and exits with
/// EXIT_SUCCESS if the startup was not aborted. processing the options has
/// global side effects (e.g. log levels, server role), so this must only be
/// called in the child process of a death test.
[[noreturn]] void startUpAndExit(bool fedRAMPHigh,
                                 std::vector<std::string> args) {
  disableCoredump();
  deactivateLogger();
  makeServer(fedRAMPHigh)->declareParseAndProcessOptions(std::move(args));
  std::_Exit(EXIT_SUCCESS);
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

TEST(FedRAMPHighEnabledTest, off_means_disabled) {
  auto result = isFedRAMPHighEnabled("off");
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.get());
}

TEST(FedRAMPHighEnabledTest, on_means_enabled) {
  auto result = isFedRAMPHighEnabled("on");
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.get());
}

// a mistyped value must never silently select a mode, so everything except
// the exact spellings "on" and "off" is invalid.
TEST(FedRAMPHighEnabledTest, any_other_value_is_invalid) {
  for (std::string_view value :
       {"", "1", "0", "true", "false", "ON", "Off", " on", "on ", "onn"}) {
    auto result = isFedRAMPHighEnabled(value);
    EXPECT_TRUE(result.fail()) << "value: '" << value << "'";
    EXPECT_EQ(result.errorNumber(), TRI_ERROR_BAD_PARAMETER)
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

TEST(ArangodServerFedRAMPTest, fedramp_high_changes_logger_option_defaults) {
  ScopedAlwaysLogSecurityEvents guard;
  EXPECT_TRUE(makeServer(true)
                  ->getOptions<LoggerOptionsProvider>()
                  .alwaysLogSecurityMessages);
}

TEST(ArangodServerFedRAMPTest, no_fedramp_high_keeps_logger_option_defaults) {
  EXPECT_FALSE(makeServer(false)
                   ->getOptions<LoggerOptionsProvider>()
                   .alwaysLogSecurityMessages);
}

// the INFO message proves that the FedRAMP High checks were run.
TEST(ArangodServerFedRAMPDeathTest, fedramp_high_starts_with_default_options) {
  EXPECT_EXIT(startUpAndExit(true, {}), ::testing::ExitedWithCode(EXIT_SUCCESS),
              "FedRAMP High mode is enabled");
}

TEST(ArangodServerFedRAMPDeathTest,
     fedramp_high_aborts_if_forced_security_messages_are_disabled) {
  EXPECT_EXIT(
      startUpAndExit(true, {"--log.always-log-security-messages", "false"}),
      ::testing::ExitedWithCode(EXIT_FAILURE),
      "'--log.always-log-security-messages' must not be set to false in "
      "FedRAMP High mode");
}

TEST(ArangodServerFedRAMPDeathTest,
     no_fedramp_high_allows_disabling_forced_security_messages) {
  EXPECT_EXIT(
      startUpAndExit(false, {"--log.always-log-security-messages", "false"}),
      ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

// in FedRAMP High mode, security events are logged right from the
// construction of the server, i.e. before options are processed.
TEST(ArangodServerFedRAMPTest, fedramp_high_forces_security_logging_early) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  EXPECT_TRUE(makeServer(true)->isFedRAMPHigh());
  EXPECT_TRUE(Logger::alwaysLogSecurityEvents());
}

TEST(ArangodServerFedRAMPTest,
     no_fedramp_high_does_not_force_security_logging) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  EXPECT_FALSE(makeServer(false)->isFedRAMPHigh());
  EXPECT_FALSE(Logger::alwaysLogSecurityEvents());
}
