////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/ResultT.h"
#include "Pregel/Collections/Collections.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel::collections {

/**
   Creates a Collection based on the server state
In the single server server state it creates a SingleServerCollection, otherwise
a ClusterCollection (in case of a smart collection, the ClusterCollection will
include all underlying collections and be named by the name of the virtual
collection).
 **/
struct CollectionFactory {
  TRI_vocbase_t& vocbase;
  CollectionFactory(TRI_vocbase_t& vocbase) : vocbase{vocbase} {}
  auto create(std::vector<CollectionID> names) -> ResultT<Collections>;
};

}  // namespace arangodb::pregel::collections
