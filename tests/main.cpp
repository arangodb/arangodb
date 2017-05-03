#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"

int main( int argc, char* argv[] )
{
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
  // global setup...
  arangodb::Logger::initialize(false);
  int result = Catch::Session().run( argc, argv );
  arangodb::Logger::shutdown();
  // global clean-up...

  return ( result < 0xff ? result : 0xff );
}
