#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/Thread.h"
#include "Basics/ConditionLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Logger/LogAppender.h"
#include "Random/RandomGenerator.h"
#include "RestServer/ServerIdFeature.h"
#include "tests/Basics/icu-helper.h"

#include <chrono>
#include <thread>

template<class Function> class TestThread : public arangodb::Thread {
public:

  TestThread(Function&& f, int i, char* c[]) :
    arangodb::Thread("catch"), _f(f), _i(i), _c(c), _done(false) {
    run();
    while (true) {
      if (_done) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
  
  void run() {
    _result = _f(_i,_c);
    _done = true;
  }
  
  int result() { return _result; }

private:
  Function _f;
  int _i;
  char** _c;
  std::atomic<bool> _done;
  std::atomic<int> _result;
};

char const* ARGV0 = "";

int main(int argc, char* argv[]) {
  ARGV0 = argv[0];
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
  // global setup...
  arangodb::Logger::initialize(false);
  arangodb::LogAppender::addAppender("-"); 

  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_SINGLE);

  arangodb::ShellColorsFeature sc(nullptr);
  sc.prepare();
  
  arangodb::ArangoGlobalContext ctx(1, const_cast<char**>(&ARGV0), ".");
  ctx.exit(0); // set "good" exit code by default
  
  arangodb::ServerIdFeature::setId(12345);
  IcuInitializer::setup(ARGV0);

  // Run tests in subthread such that it has a larger stack size in libmusl,
  // the stack size for subthreads has been reconfigured in the
  // ArangoGlobalContext above in the libmusl case:
  int result;
  auto tests = [] (int argc, char* argv[]) -> int {
    return Catch::Session().run( argc, argv );
  };
  TestThread<decltype(tests)> t(std::move(tests), argc, argv);
  result = t.result();

  arangodb::Logger::shutdown();
  // global clean-up...

  return ( result < 0xff ? result : 0xff );
}
