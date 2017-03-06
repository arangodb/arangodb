
#ifndef ARANGODB_TEST_ICU_HELPER
#define ARANGODB_TEST_ICU_HELPER 1

#include "Basics/Common.h"

struct IcuInitializer {
  IcuInitializer();
  ~IcuInitializer();

  static void setup(char const* path);

  static void* icuDataPtr;
  static bool initialized;
};

#endif
