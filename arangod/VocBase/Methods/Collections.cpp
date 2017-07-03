////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Collections.h"
#include "Basics/Common.h"

#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/Events.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-collection.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/modes.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <regex>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::methods;

void methods::Collections::enumerateCollections(
    TRI_vocbase_t* vocbase,
    std::function<void(LogicalCollection*)> const& func) {
  if (ServerState::instance()->isCoordinator()) {
    std::vector<std::shared_ptr<LogicalCollection>> colls =
        ClusterInfo::instance()->getCollections(vocbase->name());
    for (std::shared_ptr<LogicalCollection> const& c : colls) {
      if (!c->deleted()) {
        func(c.get());
      }
    }

  } else {
    std::vector<arangodb::LogicalCollection*> colls =
        vocbase->collections(false);
    for (LogicalCollection* c : colls) {
      if (!c->deleted()) {
        func(c);
      }
    }
  }
}

bool methods::Collections::lookupCollection(
    TRI_vocbase_t* vocbase, std::string const& collection,
    std::function<void(LogicalCollection*)> const& func) {
  if (!collection.empty()) {
    if (ServerState::instance()->isCoordinator()) {
      try {
        std::shared_ptr<LogicalCollection> coll =
            ClusterInfo::instance()->getCollection(vocbase->name(), collection);
        if (coll) {
          func(coll.get());
          return true;
        }
      } catch (...) {
      }
    } else {
      LogicalCollection* coll = vocbase->lookupCollection(collection);
      if (coll != nullptr) {
        func(coll);
        return true;
      }
    }
  }
  return false;
}
