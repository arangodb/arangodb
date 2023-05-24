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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "GraphVPackBuilderStorer.h"
#include <cstdint>

#include "Pregel/GraphFormat.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"

#include "Pregel/Worker/WorkerConfig.h"

#include "Logger/LogMacros.h"

#include "Scheduler/SchedulerFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

#include "ApplicationFeatures/ApplicationServer.h"

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << "[job " << config->executionNumber() << "] "

namespace arangodb::pregel {

template<typename V, typename E>
auto GraphVPackBuilderStorer<V, E>::store(std::shared_ptr<Magazine<V, E>> magazine)
    -> futures::Future<futures::Unit> {
  std::string tmp;

  for (auto& quiver : *magazine) {
    for (auto& vertex : *quiver) {
      std::string const& cname =
          config->graphSerdeConfig().collectionName(vertex.shard());
      result->openObject(/*unindexed*/ true);
      if (withId) {
        if (!cname.empty()) {
          tmp.clear();
          tmp.append(cname);
          tmp.push_back('/');
          tmp.append(vertex.key().data(), vertex.key().size());
          result->add(StaticStrings::IdString, VPackValue(tmp));
        }
      }

      result->add(StaticStrings::KeyString,
                  VPackValuePair(vertex.key().data(), vertex.key().size(),
                                 VPackValueType::String));

      V const& data = vertex.data();
      if (auto res = graphFormat->buildVertexDocument(*result, &data); !res) {
        LOG_PREGEL("37fde", ERR) << "Failed to build vertex document";
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Failed to build vertex document");
      }
      result->close();
    }
  }
  return futures::Unit{};
}

}  // namespace arangodb::pregel

template struct arangodb::pregel::GraphVPackBuilderStorer<int64_t, int64_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<uint64_t, uint64_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<uint64_t, uint8_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<float, float>;
template struct arangodb::pregel::GraphVPackBuilderStorer<double, float>;
template struct arangodb::pregel::GraphVPackBuilderStorer<double, double>;
template struct arangodb::pregel::GraphVPackBuilderStorer<float, uint8_t>;

// specific algo combos
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::WCCValue, uint64_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::SCCValue, int8_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::ECValue, int8_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::HITSValue, int8_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::HITSKleinbergValue, int8_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::DMIDValue, float>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::LPValue, int8_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::SLPAValue, int8_t>;
template struct arangodb::pregel::GraphVPackBuilderStorer<
    arangodb::pregel::algos::ColorPropagationValue, int8_t>;
