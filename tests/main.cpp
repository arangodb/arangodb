#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "Logger/Logger.h"
#include "Logger/LogAppender.h"
#include "Random/RandomGenerator.h"

char const* ARGV0 = "";

int main( int argc, char* argv[] )
{
  ARGV0 = argv[0];
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
  // global setup...
  arangodb::Logger::initialize(false);
  arangodb::LogAppender::addTtyAppender();
  int result = Catch::Session().run( argc, argv );
  arangodb::Logger::shutdown();
  // global clean-up...

  return ( result < 0xff ? result : 0xff );
}
