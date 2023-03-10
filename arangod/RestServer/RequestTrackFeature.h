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

#pragma once

#include "Basics/LruCache.h"
#include "RestServer/arangod.h"
#include "VocBase/voc-types.h"

#include <array>
#include <memory>
#include <mutex>
#include <string>

namespace arangodb {
namespace velocypack {
class Slice;
}  // namespace velocypack

class RequestTrackFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "RequestTrack"; }

  explicit RequestTrackFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  bool trackRequest(std::string const& collectionName,
                    TRI_voc_document_operation_e op, velocypack::Slice body);

 private:
  // whether or not the feature is enabled
  bool _trackRequests;

  // number of entries tracked in the LRU cache
  size_t _numEntries;

  // approximate time to live for entries in the _operations map
  double _ttl;

  // protects _operations
  std::mutex _mutex;

  // the array contains different buckets for insert / update / replace /
  // remove. in each bucket, there is a map of string (collection + hash) =>
  // timestamp
  std::array<std::unique_ptr<basics::LruCache<std::string, double>>, 5>
      _operations;
};

}  // namespace arangodb
