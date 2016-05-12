////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "InitializeRest.h"

#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/mimetypes.h"
#include "Basics/process-utils.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/Version.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// initialization
// -----------------------------------------------------------------------------

namespace arangodb {
namespace rest {
void InitializeRest() {
  TRI_InitializeMemory();
  TRI_InitializeDebugging();
  TRI_InitializeError();
  TRI_InitializeFiles();
  TRI_InitializeMimetypes();
  TRI_InitializeProcess();

  // use the rng so the linker does not remove it from the executable
  // we might need it later because .so files might refer to the symbols
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);

#ifdef TRI_BROKEN_CXA_GUARD
  pthread_cond_t cond;
  pthread_cond_init(&cond, 0);
  pthread_cond_broadcast(&cond);
#endif

  Version::initialize();
  VelocyPackHelper::initialize();
}

void ShutdownRest() {
  RandomGenerator::shutdown();

  TRI_ShutdownProcess();
  TRI_ShutdownMimetypes();
  TRI_ShutdownFiles();
  TRI_ShutdownError();
  TRI_ShutdownDebugging();
  TRI_ShutdownMemory();
}
}
}
