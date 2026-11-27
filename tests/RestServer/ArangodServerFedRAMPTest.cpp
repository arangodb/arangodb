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
#include "Mocks/TestLogger.h"

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

/// @brief runs the option handling of the server startup and exits with
/// EXIT_SUCCESS if the startup was not aborted. processing the options has
/// global side effects (e.g. log levels, server role), so this must only be
/// called in the child process of a death test.
[[noreturn]] void startUpAndExit(bool fedRAMPHigh,
                                 std::vector<std::string> args) {
  disableCoredump();
  // as in the early server startup, the logger is inactive. it then writes to
  // stderr, which is what death tests can match.
  Logger::shutdown();
  makeServer(fedRAMPHigh)->declareParseAndProcessOptions(std::move(args));
  std::_Exit(EXIT_SUCCESS);
}

/// @brief parses `args` as command line with the real logger option
/// declarations.
LoggerOptions parseLoggerOptions(std::vector<std::string> args) {
  LoggerOptionsProvider provider;
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

TEST(FedRAMPHighLoggerOptionsTest, option_forces_security_messages) {
  // the default is false, so that true can only come from the option
  EXPECT_TRUE(parseLoggerOptions({"--log.always-log-security-messages", "true"})
                  .alwaysLogSecurityMessages);
}

TEST(FedRAMPHighLoggerOptionsTest, check_requires_forced_security_messages) {
  LoggerOptions options;
  options.alwaysLogSecurityMessages = true;
  EXPECT_TRUE(checkFedRAMPHighLoggerOptions(options).ok());

  options.alwaysLogSecurityMessages = false;
  auto result = checkFedRAMPHighLoggerOptions(options);
  EXPECT_TRUE(result.fail());
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_BAD_PARAMETER);
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

TEST(ArangodServerFedRAMPDeathTest,
     fedramp_high_aborts_if_forced_security_messages_are_disabled) {
  EXPECT_EXIT(
      startUpAndExit(true, {"--log.always-log-security-messages", "false"}),
      ::testing::ExitedWithCode(EXIT_FAILURE),
      "'--log.always-log-security-messages' must not be set to false in "
      "FedRAMP High mode");
}

// in FedRAMP High mode, security events are logged right from the
// construction of the server, i.e. before options are processed.
TEST(ArangodServerFedRAMPTest, fedramp_high_forces_security_logging_early) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  makeServer(true);
  EXPECT_TRUE(Logger::alwaysLogSecurityEvents());
}

TEST(ArangodServerFedRAMPTest,
     no_fedramp_high_does_not_force_security_logging) {
  ScopedAlwaysLogSecurityEvents guard;
  Logger::setAlwaysLogSecurityEvents(false);
  makeServer(false);
  EXPECT_FALSE(Logger::alwaysLogSecurityEvents());
}
