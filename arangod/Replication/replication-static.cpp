////////////////////////////////////////////////////////////////////////////////
/// @brief replication data fetcher
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "replication-static.h"
#include "Replication/ContinuousSyncer.h"

#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief static create method
////////////////////////////////////////////////////////////////////////////////

void* TRI_CreateContinuousSyncerReplication (TRI_vocbase_t* vocbase,
                                             TRI_replication_applier_configuration_t const* configuration,
                                             TRI_voc_tick_t initialTick,
                                             bool useTick) {

  ContinuousSyncer* s = new ContinuousSyncer(vocbase, 
                                             configuration, 
                                             initialTick,
                                             useTick);

  return (void*) s;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief static free method
////////////////////////////////////////////////////////////////////////////////

void TRI_DeleteContinuousSyncerReplication (void* ptr) {
  ContinuousSyncer* s = static_cast<ContinuousSyncer*>(ptr);

  delete s;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief static run method
////////////////////////////////////////////////////////////////////////////////

int TRI_RunContinuousSyncerReplication (void* ptr) {
  ContinuousSyncer* s = static_cast<ContinuousSyncer*>(ptr);

  return s->run();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
