////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RequestTrackFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/debugging.h"
#include "Basics/system-functions.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::options;

RequestTrackFeature::RequestTrackFeature(Server& server)
    : ArangodFeature{server, *this},
      _trackRequests(false),
      _numEntries(16384),
      _ttl(300.0) {
  startsAfter<application_features::DatabaseFeaturePhase>();
  startsAfter<application_features::ServerFeaturePhase>();

  // ensure that we will have enough buckets. would be better if
  // std::array<...>::size() would be a static member, so we could use
  // it in a static_assert
  TRI_ASSERT(_operations.size() > TRI_VOC_DOCUMENT_OPERATION_INSERT);
  TRI_ASSERT(_operations.size() > TRI_VOC_DOCUMENT_OPERATION_UPDATE);
  TRI_ASSERT(_operations.size() > TRI_VOC_DOCUMENT_OPERATION_REPLACE);
  TRI_ASSERT(_operations.size() > TRI_VOC_DOCUMENT_OPERATION_REMOVE);
}

void RequestTrackFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options
      ->addOption("--server.track-duplicate-document-requests",
                  "If enabled, tracks duplicate requests to document "
                  "modification APIs.",
                  new BooleanParameter(&_trackRequests),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(31100)
      .setLongDescription(
          R"(If enabled, tracks duplicate requests to modification
operations in the document CRUD API. Requests are identified by collection/shard
and based on the request body's hash. 
Enabling this option is only useful for debugging very specific situations and
should otherwise be avoided. Enabling the option can also have a negative impact
on performance and throughput.)");

  options
      ->addOption(
          "--server.track-duplicate-document-requests-cache-size",
          "Number of entries in the cache for tracking duplicate requests.",
          new SizeTParameter(&_numEntries),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31100);

  options
      ->addOption("--server.track-duplicate-document-requests-ttl",
                  "Period (in seconds) in which 2 requests will be counted as "
                  "duplicates.",
                  new DoubleParameter(&_ttl),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(31100);
}

bool RequestTrackFeature::trackRequest(std::string const& collectionName,
                                       TRI_voc_document_operation_e op,
                                       velocypack::Slice body) {
  if (_trackRequests) {
    // build lookup key from collection name and hash of body
    std::string key =
        collectionName + "-" + std::to_string(body.normalizedHash());

    // get current time
    double now = TRI_microtime();

    // find target bucket (insert / update / replace / remove)
    auto& bucket = _operations[op];

    std::unique_lock<std::mutex> guard(_mutex);

    if (bucket == nullptr) {
      bucket =
          std::make_unique<basics::LruCache<std::string, double>>(_numEntries);
    }
    TRI_ASSERT(bucket != nullptr);

    auto* value = bucket->get(key);
    if (value != nullptr) {
      // found a previous entry
      bool expired = *value < now;
      // need to remove and reinsert to update LRU
      bucket->remove(key);
      bucket->put(key, now + _ttl);
      return !expired;
    }
    // new entry
    bucket->put(key, now + _ttl);
  }

  return false;
}
