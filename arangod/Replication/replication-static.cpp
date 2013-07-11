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
#include "Replication/ReplicationFetcher.h"

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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief static factory method
////////////////////////////////////////////////////////////////////////////////

void* TRI_CreateFetcherReplication (TRI_vocbase_t* vocbase,
                                    const char* masterEndpoint,
                                    double timeout) {
  ReplicationFetcher* f = new ReplicationFetcher(vocbase, string(masterEndpoint), timeout);

  return (void*) f;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief static run method
////////////////////////////////////////////////////////////////////////////////

int TRI_RunFetcherReplication (void* ptr,
                               bool forceFullSynchronisation,
                               uint64_t ignoreCount) {
  ReplicationFetcher* f = static_cast<ReplicationFetcher*>(ptr);

  string errorMsg;
  int res = f->run(forceFullSynchronisation, ignoreCount, errorMsg);

  return res;
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
