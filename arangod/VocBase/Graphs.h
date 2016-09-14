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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_GRAPHS_H
#define ARANGOD_VOCBASE_GRAPHS_H 1

#include "VocBase/vocbase.h"

namespace arangodb {
namespace aql {
class Graph;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an instance of Graph by Name.
///  returns nullptr if graph is not existing
///  The caller has to take care for the memory.
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::Graph* lookupGraphByName(TRI_vocbase_t*, std::string const&);

}  // namespace arangodb

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|//
// --SECTION--\\|/// @\\}"
