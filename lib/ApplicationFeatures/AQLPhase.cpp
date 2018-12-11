////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "AQLPhase.h"

namespace arangodb {
namespace application_features {

AQLFeaturePhase::AQLFeaturePhase(ApplicationServer& server)
    : ApplicationFeaturePhase(server, "AQLPhase") {
  setOptional(false);
  startsAfter("V8Phase");

  startsAfter("CommunicationPhase");
  startsAfter("Aql");
  startsAfter("AQLFunctions");
  startsAfter("IResearchAnalyzer");
  startsAfter("ArangoSearch");
  startsAfter("OptimizerRules");
  startsAfter("Pregel");
  startsAfter("QueryRegistry");
  startsAfter("SystemDatabase");
  startsAfter("TraverserEngineRegistry");
}

} // application_features
} // arangodb
