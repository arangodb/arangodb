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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/Worker/Worker.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace pregel {

class PregelFeature;

struct AlgoRegistry {
  static IAlgorithm* createAlgorithm(std::string const& algorithm,
                                     VPackSlice userParams);
  static std::shared_ptr<IWorker> createWorker(TRI_vocbase_t& vocbase,
                                               CreateWorker const& parameters,
                                               PregelFeature& feature);
  static auto createAlgorithmNew(std::string const& algorithm,
                                 VPackSlice userParams)
      -> std::optional<std::unique_ptr<IAlgorithm>>;

 private:
  template<typename V, typename E, typename M>
  static std::shared_ptr<IWorker> createWorker(TRI_vocbase_t& vocbase,
                                               Algorithm<V, E, M>* algo,
                                               CreateWorker const& parameters,
                                               PregelFeature& feature);
};

}  // namespace pregel
}  // namespace arangodb
