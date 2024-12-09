////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndexFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Parameters.h"

namespace arangodb {

VectorIndexFeature::VectorIndexFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

void VectorIndexFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--experimental-vector-index",
                  "Turn on experimental vector index feature.",
                  new options::BooleanParameter(&_useVectorIndex),
                  options::makeFlags())
      .setIntroducedIn(31204)
      .setLongDescription(
          R"(Turn on experimental vector index features. If this is enabled
downgrading from this version will no longer be possible.)");
}

bool VectorIndexFeature::isVectorIndexEnabled() const {
  return _useVectorIndex;
}

}  // namespace arangodb
