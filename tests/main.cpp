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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <thread>

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Basics/icu-helper.h"
#include "Cluster/ServerState.h"
#include "Logger/LogAppender.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"
#include "VocBase/Identifiers/ServerId.h"

template <class Function>
class TestThread : public arangodb::Thread {
 public:
  TestThread(arangodb::application_features::ApplicationServer& server,
             Function&& f, int i, char* c[])
      : arangodb::Thread(server, "gtest"), _f(f), _i(i), _c(c), _done(false) {
    run();
    CONDITION_LOCKER(guard, _wait);
    while (true) {
      if (_done) {
        break;
      }
      _wait.wait(uint64_t(1000000));
    }
  }
  ~TestThread() { shutdown(); }

  void run() override {
    CONDITION_LOCKER(guard, _wait);
    _result = _f(_i, _c);
    _done = true;
    _wait.broadcast();
  }

  int result() { return _result; }

 private:
  Function _f;
  int _i;
  char** _c;
  std::atomic<bool> _done;
  std::atomic<int> _result;
  arangodb::basics::ConditionVariable _wait;
};

char const* ARGV0 = "";

namespace arangodb {
  // Only to please the linker, this is not used in the tests.
  std::function<int()>* restartAction;
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  TRI_GET_ARGV(argc, argv);
  int subargc = 0;
  char** subargv = (char**)malloc(sizeof(char*) * argc);
  bool logLineNumbers = false;
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
  // global setup...
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      arangodb::rest::Version::initialize();
      std::cout << arangodb::rest::Version::getServerVersion() << std::endl
                << std::endl
                << arangodb::rest::Version::getDetailed() << std::endl;
      exit(EXIT_SUCCESS);
    }
    if (strcmp(argv[i], "--log.line-number") == 0) {
      if (i < argc) {
        i++;
        if (i < argc) {
          if (strcmp(argv[i], "true") == 0) {
            logLineNumbers = true;
          }
          i++;
        }
      }
    } else {
      subargv[subargc] = argv[i];
      subargc++;
    }
  }

  ARGV0 = subargv[0];

  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::ServerState state(server);
  state.setRole(arangodb::ServerState::ROLE_SINGLE);
  arangodb::ShellColorsFeature sc(server);

  arangodb::Logger::setShowLineNumber(logLineNumbers);
  arangodb::Logger::initialize(server, false);
  arangodb::LogAppender::addAppender(arangodb::Logger::defaultLogGroup(), "-");

  sc.prepare();

  arangodb::ArangoGlobalContext ctx(1, const_cast<char**>(&ARGV0), ".");
  ctx.exit(0);  // set "good" exit code by default

  arangodb::ServerIdFeature::setId(arangodb::ServerId{12345});
  IcuInitializer::setup(ARGV0);

  // Run tests in subthread such that it has a larger stack size in libmusl,
  // the stack size for subthreads has been reconfigured in the
  // ArangoGlobalContext above in the libmusl case:
  int result;
  auto tests = [](int argc, char* argv[]) -> int {
    return RUN_ALL_TESTS();
  };
  TestThread<decltype(tests)> t(server, std::move(tests), subargc, subargv);
  result = t.result();

  arangodb::Logger::shutdown();
  // global clean-up...
  free(subargv);
  return (result < 0xff ? result : 0xff);
}
