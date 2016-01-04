////////////////////////////////////////////////////////////////////////////////
/// @brief work monitor dummy class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "WorkMonitor.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                 class WorkMonitor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief thread deleter
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::DELETE_HANDLER (WorkDescription*) {
  TRI_ASSERT(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread description string
////////////////////////////////////////////////////////////////////////////////

void WorkMonitor::VPACK_HANDLER (VPackBuilder*, WorkDescription*) {
  TRI_ASSERT(false);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
