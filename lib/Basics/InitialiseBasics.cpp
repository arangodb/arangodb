////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into programm
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/InitialiseBasics.h"

#include "Basics/init.h"
#include "Basics/error.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Basics/random.h"
#include "Basics/RandomGenerator.h"

#ifdef TRI_BROKEN_CXA_GUARD
#include <pthread.h>
#endif


namespace triagens {
  namespace basics {
    void InitialiseBasics (int argv, char* argc[]) {
      TRIAGENS_C_INITIALISE(argv, argc);

      // use the rng so the linker does not remove it from the executable
      // we might need it later because .so files might refer to the symbols
      Random::random_e v = Random::selectVersion(Random::RAND_MERSENNE);
      Random::UniformInteger random(0, INT32_MAX);
      random.random();
      Random::selectVersion(v);

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
