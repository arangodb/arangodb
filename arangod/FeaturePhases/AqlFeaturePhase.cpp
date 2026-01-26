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
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "AqlFeaturePhase.h"

#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"

namespace arangodb::application_features {

AqlFeaturePhase::AqlFeaturePhase(ArangodServer& server)
    : ApplicationFeaturePhase{server, *this} {
  setOptional(false);
  startsAfter<CommunicationFeaturePhase>();
#ifdef USE_V8
  startsAfter<V8FeaturePhase>();
#endif

  startsAfter<AqlFeature>();
  startsAfter<aql::AqlFunctionFeature>();
  startsAfter<iresearch::IResearchAnalyzerFeature>();
  startsAfter<iresearch::IResearchFeature>();
  startsAfter<aql::OptimizerRulesFeature>();
  startsAfter<QueryRegistryFeature>();
  startsAfter<SystemDatabaseFeature>();
}

}  // namespace arangodb::application_features
