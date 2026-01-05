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
/// Helper
Collection* addCollectionToQuery(QueryContext& query, std::string const& cname,
                                 char const* context);

void insertDistributeInputCalculation(ExecutionPlan& plan);

void activateCallstackSplit(ExecutionPlan& plan);

/// @brief propagate constant attributes in FILTERs
void propagateConstantAttributesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const&);

/// @brief split and-combined filters and break them into smaller parts
void splitFiltersRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                      OptimizerRule const&);

/// @brief replace single document operations in cluster by special handling
void substituteClusterSingleDocumentOperationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan, OptimizerRule const&);

/// @brief replace multiple document operations in cluster by special handling
void substituteClusterMultipleDocumentOperationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan, OptimizerRule const&);

#ifdef USE_ENTERPRISE
/// @brief optimize queries in the cluster so that the entire query gets pushed
/// to a single server
void clusterOneShardRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                         OptimizerRule const&);
#endif

#ifdef USE_ENTERPRISE
void clusterLiftConstantsForDisjointGraphNodes(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule);
#endif

#ifdef USE_ENTERPRISE
void clusterPushSubqueryToDBServer(Optimizer* opt,
                                   std::unique_ptr<ExecutionPlan> plan,
                                   OptimizerRule const& rule);
#endif

#ifdef USE_ENTERPRISE
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
#endif

// replace inaccessible EnumerateCollectionNode with NoResult nodes
#ifdef USE_ENTERPRISE
void skipInaccessibleCollectionsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const& rule);
#endif

/// @brief replace legacy JS functions in the plan.
void replaceNearWithinFulltextRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                   OptimizerRule const&);

/// @brief replace LIKE function with range scan where possible
void replaceLikeWithRangeRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                              OptimizerRule const&);

/// @brief replace enumeration of ENTRIES with object iteration
void replaceEntriesWithObjectIteration(Optimizer*,
                                       std::unique_ptr<ExecutionPlan>,
                                       OptimizerRule const&);

void joinIndexNodesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                        OptimizerRule const&);

void replaceEqualAttributeAccesses(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                   OptimizerRule const&);

void batchMaterializeDocumentsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                   OptimizerRule const&);

void pushDownLateMaterializationRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const&);

void materializeIntoSeparateVariable(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const&);

void pushLimitIntoIndexRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                            OptimizerRule const&);

void useVectorIndexRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                        OptimizerRule const&);

void pushFilterIntoEnumerateNear(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                 OptimizerRule const&);

void useIndexForCollect(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                        OptimizerRule const& rule);

}  // namespace arangodb::aql
