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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Containers/SmallUnorderedMap.h"
#include "VocBase/vocbase.h"

namespace arangodb::aql {
class Optimizer;
class ExecutionNode;
class SubqueryNode;

class QueryContext;
struct Collection;

/// @brief split and-combined filters and break them into smaller parts
void splitFiltersRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                      OptimizerRule const&);

#ifdef USE_ENTERPRISE
/// @brief optimize queries in the cluster so that the entire query gets pushed
/// to a single server
void clusterOneShardRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                         OptimizerRule const&);

void clusterLiftConstantsForDisjointGraphNodes(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule);

void clusterPushSubqueryToDBServer(Optimizer* opt,
                                   std::unique_ptr<ExecutionPlan> plan,
                                   OptimizerRule const& rule);

void distributeOffsetInfoToClusterRule(aql::Optimizer* opt,
                                       std::unique_ptr<aql::ExecutionPlan> plan,
                                       aql::OptimizerRule const& rule);

void lateMaterialiationOffsetInfoRule(aql::Optimizer* opt,
                                      std::unique_ptr<aql::ExecutionPlan> plan,
                                      aql::OptimizerRule const& rule);

/// @brief remove scatter/gather and remote nodes for SatelliteCollections
void scatterSatelliteGraphRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                               OptimizerRule const&);

/// @brief remove scatter/gather and remote nodes for SatelliteCollections
void removeSatelliteJoinsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                              OptimizerRule const&);

/// @brief remove distribute/gather and remote nodes if possible
void removeDistributeNodesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                               OptimizerRule const&);

void smartJoinsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                    OptimizerRule const&);

// replace inaccessible EnumerateCollectionNode with NoResult nodes
void skipInaccessibleCollectionsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const& rule);
#endif
}  // namespace arangodb::aql
