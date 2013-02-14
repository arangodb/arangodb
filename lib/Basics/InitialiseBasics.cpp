////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into programm
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/InitialiseBasics.h"

#include "BasicsC/init.h"
#include "BasicsC/error.h"
#include "BasicsC/hashes.h"
#include "BasicsC/random.h"

#include "Logger/Logger.h"
#include "Basics/RandomGenerator.h"

#ifdef TRI_BROKEN_CXA_GUARD
#include <pthread.h>
#endif

#include <build.h>

namespace triagens {
  namespace basics {
    void InitialiseBasics (int argv, char* argc[]) {
      TRIAGENS_C_INITIALISE(argv, argc);

      // use the rng so the linker does not remove it from the executable
      // we might need it later because .so files might refer to the symbols
      Random::random_e v = Random::selectVersion(Random::RAND_MERSENNE);
      Random::UniformInteger random(0, 1);
      random.random();
      Random::selectVersion(v);

      string revision = "$Revision: BASICS " TRIAGENS_VERSION " (c) triAGENS GmbH $";
      LOGGER_TRACE(revision);

#ifdef TRI_BROKEN_CXA_GUARD
      pthread_cond_t cond;
      pthread_cond_init(&cond, 0);
      pthread_cond_broadcast(&cond);
#endif
    }



    void ShutdownBasics () {
      TRIAGENS_C_SHUTDOWN;
    }
  }
}

