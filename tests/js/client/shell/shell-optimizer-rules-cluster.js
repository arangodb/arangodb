/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief queries for all databases tests
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');

const expectedRules = [
  {
    "name": "replace-function-with-index",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": false,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "inline-subqueries",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "simplify-conditions",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "move-calculations-up",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "move-filters-up",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-redundant-calculations",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-unnecessary-filters",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-unnecessary-calculations",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "specialize-collect",
    "flags": {
      "hidden": true,
      "clusterOnly": false,
      "canBeDisabled": false,
      "canCreateAdditionalPlans": true,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-redundant-sorts",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "optimize-subqueries",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "interchange-adjacent-enumerations",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": true,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "move-calculations-up-2",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "move-filters-up-2",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-redundant-sorts-2",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-sort-rand-limit-1",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-collect-variables",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "propagate-constant-attributes",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-data-modification-out-variables",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "replace-or-with-in",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-redundant-or",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "geo-index-optimizer",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "use-indexes",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-filter-covered-by-index",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-unnecessary-filters-2",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "use-index-for-sort",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "sort-in-values",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "optimize-traversals",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-filter-covered-by-traversal",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "handle-arangosearch-views",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": false,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-unnecessary-calculations-2",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-redundant-path-var",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "optimize-cluster-single-document-operations",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "move-calculations-down",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "fuse-filters",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "cluster-one-shard",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": true
    }
  },
  {
    "name": "cluster-lift-constant-for-disjoint-graph-nodes",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": false,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": true
    }
  },
  {
    "name": "distribute-in-cluster",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": false,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "smart-joins",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": true
    }
  },
  {
    "name": "scatter-in-cluster",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": false,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "scatter-satellite-graphs",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": true
    }
  },
  {
    "name": "remove-satellite-joins",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": true
    }
  },
  {
    "name": "remove-distribute-nodes",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": true
    }
  },
  {
    "name": "distribute-filtercalc-to-cluster",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "distribute-sort-to-cluster",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-unnecessary-calculations-3",
    "flags": {
      "hidden": true,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": true,
      "enterpriseOnly": false
    }
  },
  {
    "name": "remove-unnecessary-remote-scatter",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "undistribute-remove-after-enum-coll",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "collect-in-cluster",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "sort-limit",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "restrict-to-single-shard",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "reduce-extraction-to-projection",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "move-filters-into-enumerate",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "optimize-count",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "parallelize-gather",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "async-prefetch",
    "flags": {
      "hidden": true,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": true,
      "enterpriseOnly": false
    }
  },
  {
    "name": "decay-unnecessary-sorted-gather",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "push-subqueries-to-dbserver",
    "flags": {
      "hidden": false,
      "clusterOnly": true,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": true
    }
  },
  {
    "name": "late-document-materialization-arangosearch",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "late-document-materialization",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": true,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  },
  {
    "name": "splice-subqueries",
    "flags": {
      "hidden": false,
      "clusterOnly": false,
      "canBeDisabled": false,
      "canCreateAdditionalPlans": false,
      "disabledByDefault": false,
      "enterpriseOnly": false
    }
  }
];


function optimizerRulesClusterSuite() {
  'use strict';
  return {

    testDisplayingOfAllRules: function () {
      for (let i = 0; i < 10; ++i) {
        let response = arango.GET("/_api/query/rules");
        let clusterOnlyRules = expectedRules.filter(rule => rule.flags.clusterOnly === true);
        let nonClusterOnlyRules = expectedRules.filter(rule => rule.flags.clusterOnly === false);
        assertEqual(response.length, expectedRules.length);
        nonClusterOnlyRules.forEach(nonClusterRule => {
          const index = response.findIndex(rule => rule.name === nonClusterRule.name);
          assertNotEqual(index, -1);
          for (const [key, value] of Object.entries(nonClusterRule.flags)) {
            assertEqual(response[index].flags[key], nonClusterRule.flags[key]);
            assertEqual(response[index].flags[value], nonClusterRule.flags[value]);
          }
        });
        clusterOnlyRules.forEach(clusterRule => {
          const index = response.findIndex(rule => rule.name === clusterRule.name);
          assertNotEqual(index, -1);
          for (const [key, value] of Object.entries(clusterRule.flags)) {
            assertEqual(response[index].flags[key], clusterRule.flags[key]);
            assertEqual(response[index].flags[value], clusterRule.flags[value]);
          }
        });
      }
    },
  };
}

jsunity.run(optimizerRulesClusterSuite);

return jsunity.done();
