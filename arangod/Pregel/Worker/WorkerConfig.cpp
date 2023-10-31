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

#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Conductor/Messages.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/Algorithm.h"
#include "Pregel/PregelFeature.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::pregel;

WorkerConfig::WorkerConfig(TRI_vocbase_t* vocbase) : _vocbase(vocbase) {}

std::string const& WorkerConfig::database() const { return _vocbase->name(); }

void WorkerConfig::updateConfig(worker::message::CreateWorker const& params) {
  _executionNumber = params.executionNumber;
  _coordinatorId = params.coordinatorId;
  _parallelism = params.parallelism;
  _graphSerdeConfig = params.graphSerdeConfig;
}

VertexID WorkerConfig::documentIdToPregel(std::string_view documentID) const {
  size_t pos = documentID.find('/');
  if (pos == std::string::npos) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not a valid document id");
  }
  std::string_view docRef(documentID);
  std::string_view collPart = docRef.substr(0, pos);
  std::string_view keyPart = docRef.substr(pos + 1);

  if (!ServerState::instance()->isRunningInCluster()) {
    return VertexID(_graphSerdeConfig.pregelShard(std::string{collPart}),
                    std::string(keyPart));
  } else {
    VPackBuilder partial;
    partial.openObject();
    partial.add(
        StaticStrings::KeyString,
        VPackValuePair(keyPart.data(), keyPart.size(), VPackValueType::String));
    partial.close();
    auto& ci = vocbase()->server().getFeature<ClusterFeature>().clusterInfo();
    auto info = ci.getCollectionNT(database(), collPart);

    ShardID responsibleShard;
    info->getResponsibleShard(partial.slice(), false, responsibleShard);

    PregelShard source = this->_graphSerdeConfig.pregelShard(responsibleShard);
    return VertexID(source, std::string(keyPart));
  }
}
