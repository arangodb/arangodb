#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Logger/LogAppender.h"
#include "Random/RandomGenerator.h"
#include "RestServer/ServerIdFeature.h"
#include "tests/Basics/icu-helper.h"

#include <thread>

char const* ARGV0 = "";

int testResult;

void runTests(int argc, char* argv[]) {
  testResult = Catch::Session().run( argc, argv );
}

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
  std::thread subthread(runTests, argc, argv);
  subthread.join();

  arangodb::Logger::shutdown();
  // global clean-up...

  return ( testResult < 0xff ? testResult : 0xff );
}
