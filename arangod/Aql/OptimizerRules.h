////////////////////////////////////////////////////////////////////////////////
/// @brief rules for the query optimizer
///
/// @file 
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_OPTIMIZER_RULES_H
#define ARANGOD_AQL_OPTIMIZER_RULES 1

#include <Basics/Common.h>

#include "Aql/Optimizer.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                           rules for the optimizer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
////////////////////////////////////////////////////////////////////////////////

    int removeRedundantSortsRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all unnecessary filters
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
////////////////////////////////////////////////////////////////////////////////

    int removeUnnecessaryFiltersRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief specialize the variables used in a COLLECT INTO
////////////////////////////////////////////////////////////////////////////////
    
    int specializeCollectVariables (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove INTO of a COLLECT if not used
////////////////////////////////////////////////////////////////////////////////
    
    int removeCollectIntoRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief propagate constant attributes in FILTERs
////////////////////////////////////////////////////////////////////////////////
    
    int propagateConstantAttributesRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove SORT RAND() if appropriate
////////////////////////////////////////////////////////////////////////////////
    
    int removeSortRandRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief move calculations up in the plan
/// this rule modifies the plan in place
/// it aims to move up calculations as far up in the plan as possible, to 
/// avoid redundant calculations in inner loops
////////////////////////////////////////////////////////////////////////////////

    int moveCalculationsUpRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief move calculations down in the plan
/// this rule modifies the plan in place
/// it aims to move down calculations as far down in the plan as possible, 
/// beyond FILTER and LIMIT statements 
////////////////////////////////////////////////////////////////////////////////

    int moveCalculationsDownRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief fuse multiple calculations into a single one
/// this rule modifies the plan in place
////////////////////////////////////////////////////////////////////////////////

    int fuseCalculationsRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the "right" type of AggregateNode and 
/// add a sort node for each COLLECT (may be removed later) 
/// this rule cannot be turned off (otherwise, the query result might be wrong!)
////////////////////////////////////////////////////////////////////////////////
 
    int specializeCollectRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*); 

////////////////////////////////////////////////////////////////////////////////
/// @brief split and-combined filters and break them into smaller parts
////////////////////////////////////////////////////////////////////////////////

    int splitFiltersRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief move filters up in the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// filters are not pushed beyond limits
////////////////////////////////////////////////////////////////////////////////

    int moveFiltersUpRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove redundant CalculationNodes
////////////////////////////////////////////////////////////////////////////////

    int removeRedundantCalculationsRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove CalculationNodes and SubqueryNodes that are never needed
////////////////////////////////////////////////////////////////////////////////

    int removeUnnecessaryCalculationsRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief useIndex, try to use an index for filtering
////////////////////////////////////////////////////////////////////////////////

    int useIndexesRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief try to use the index for sorting
////////////////////////////////////////////////////////////////////////////////

    int useIndexForSortRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief try to remove filters which are covered by indexes
////////////////////////////////////////////////////////////////////////////////

    int removeFiltersCoveredByIndexRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief interchange adjacent EnumerateCollectionNodes in all possible ways
////////////////////////////////////////////////////////////////////////////////

    int interchangeAdjacentEnumerationsRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief scatter operations in cluster - send all incoming rows to all remote
/// clients
////////////////////////////////////////////////////////////////////////////////

    int scatterInClusterRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief distribute operations in cluster - send each incoming row to every
/// remote client precisely once. This happens in queries like: 
///
///   FOR x IN coll1 [SOME FILTER/CALCULATION STATEMENTS] REMOVE f(x) in coll2
/// 
/// where coll2 is sharded by _key, but not if it is sharded by anything else. 
/// The collections coll1 and coll2 do not have to be distinct for this. 
////////////////////////////////////////////////////////////////////////////////
    
    int distributeInClusterRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

    int distributeFilternCalcToClusterRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

    int distributeSortToClusterRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
////////////////////////////////////////////////////////////////////////////////

    int removeUnnecessaryRemoteScatterRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief this rule removes Remote-Gather-Scatter/Distribute-Remote nodes from
/// plans arising from queries of the form: 
/// 
///   FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE x IN coll
/// 
/// when coll is any collection sharded by any attributes, and 
///
///   FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE x._key IN coll
///
/// when the coll is sharded by _key.
/// 
/// This rule dues not work for queries like:
/// 
///    FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE y IN coll
///
/// [different variable names], or 
///
///   FOR x IN coll1 [SOME FILTER/CALCULATION STATEMENTS] REMOVE x in coll2
///  
///  when coll1 and coll2 are not the same, or 
///
///   FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE f(x) in coll
/// 
///  where f is some function.
/// 
////////////////////////////////////////////////////////////////////////////////

    int undistributeRemoveAfterEnumCollRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief this rule replaces expressions of the type: 
///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to the
//  same (single) attribute.
////////////////////////////////////////////////////////////////////////////////

    int replaceOrWithInRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);
    
    int removeRedundantOrRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove $OLD and $NEW variables from data-modification statements
/// if not required
////////////////////////////////////////////////////////////////////////////////

    int removeDataModificationOutVariablesRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief patch UPDATE statement on single collection that iterates over the
/// entire collection to operate in batches
////////////////////////////////////////////////////////////////////////////////

    int patchUpdateStatementsRule (Optimizer*, ExecutionPlan*, Optimizer::Rule const*);
    
////////////////////////////////////////////////////////////////////////////////
/// @brief merges filter nodes into graph traversal nodes
////////////////////////////////////////////////////////////////////////////////

    int mergeFilterIntoTraversal (Optimizer* opt, ExecutionPlan* plan, Optimizer::Rule const* rule);

  }  // namespace aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

