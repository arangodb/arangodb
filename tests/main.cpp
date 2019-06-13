#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Thread.h"
#include "Cluster/ServerState.h"
#include "Logger/LogAppender.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RestServer/ServerIdFeature.h"
#include "gtest/gtest.h"
#include "tests/Basics/icu-helper.h"

#include <chrono>
#include <thread>

template <class Function>
class TestThread : public arangodb::Thread {
 public:
  TestThread(Function&& f, int i, char* c[])
      : arangodb::Thread("gtest"), _f(f), _i(i), _c(c), _done(false) {
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

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  TRI_GET_ARGV(argc, argv);
  int subargc = 0;
  char** subargv = (char**)malloc(sizeof(char*) * argc);
  bool logLineNumbers = false;
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
  // global setup...
  for (int i = 0; i < argc; i++) {
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
  arangodb::Logger::setShowLineNumber(logLineNumbers);
  arangodb::Logger::initialize(false);
  arangodb::LogAppender::addAppender("-");

  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_SINGLE);
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::ShellColorsFeature sc(server);

  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  sc.prepare();

  arangodb::ArangoGlobalContext ctx(1, const_cast<char**>(&ARGV0), ".");
  ctx.exit(0);  // set "good" exit code by default

  arangodb::ServerIdFeature::setId(12345);
  IcuInitializer::setup(ARGV0);

  // Run tests in subthread such that it has a larger stack size in libmusl,
  // the stack size for subthreads has been reconfigured in the
  // ArangoGlobalContext above in the libmusl case:
  int result;
  auto tests = [](int argc, char* argv[]) -> int {
    return RUN_ALL_TESTS();
  };
  TestThread<decltype(tests)> t(std::move(tests), subargc, subargv);
  result = t.result();

  arangodb::Logger::shutdown();
  // global clean-up...
  free(subargv);
  return (result < 0xff ? result : 0xff);
}
