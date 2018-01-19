#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

char const* ARGV0 = "";

int main(int argc, char* argv[]) {
  ARGV0 = argv[0];
  int result = Catch::Session().run( argc, argv );
  return ( result < 0xff ? result : 0xff );
}
