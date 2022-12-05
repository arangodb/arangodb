/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertNotEqual, arango */

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

const jsunity = require('jsunity');
const internal = require('internal');
const isCluster = internal.isCluster();

const expectedRules = [
  {
    name: "move-calculations-up",
    flags: {
      hidden: false,
      clusterOnly: false,
      canBeDisabled: true,
      canCreateAdditionalPlans: false,
      disabledByDefault: false,
      enterpriseOnly: false
    }
  },
  {
    name: "move-filters-up",
    flags: {
      hidden: false,
      clusterOnly: false,
      canBeDisabled: true,
      canCreateAdditionalPlans: false,
      disabledByDefault: false,
      enterpriseOnly: false
    }
  },
  {
    name: "remove-redundant-calculations",
    flags: {
      hidden: false,
      clusterOnly: false,
      canBeDisabled: true,
      canCreateAdditionalPlans: false,
      disabledByDefault: false,
      enterpriseOnly: false
    }
  },
  {
    name: "remove-unnecessary-calculations",
    flags: {
      hidden: false,
      clusterOnly: false,
      canBeDisabled: true,
      canCreateAdditionalPlans: false,
      disabledByDefault: false,
      enterpriseOnly: false
    }
  },
  {
    name: "optimize-subqueries",
    flags: {
      hidden: false,
      clusterOnly: false,
      canBeDisabled: true,
      canCreateAdditionalPlans: false,
      disabledByDefault: false,
      enterpriseOnly: false
    }
  },
  {
    name: "optimize-cluster-single-document-operations",
    flags: {
      hidden: false,
      clusterOnly: true,
      canBeDisabled: true,
      canCreateAdditionalPlans: false,
      disabledByDefault: false,
      enterpriseOnly: false
    }
  },
];


function optimizerRulesSuite() {
  'use strict';
  return {

    testDisplayingOfAllRules: function () {
      let response = arango.GET("/_api/query/rules");
      let clusterOnlyRules = expectedRules.filter(rule => rule.flags.clusterOnly === true);
      let nonClusterOnlyRules = expectedRules.filter(rule => rule.flags.clusterOnly === false);
      assertTrue(clusterOnlyRules.length > 0);
      assertTrue(nonClusterOnlyRules.length > 0);
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
        if (isCluster) {
          assertNotEqual(index, -1);
          for (const [key, value] of Object.entries(clusterRule.flags)) {
            assertEqual(response[index].flags[key], clusterRule.flags[key]);
            assertEqual(response[index].flags[value], clusterRule.flags[value]);
          }
        } else {
          assertEqual(index, -1);
        }
      });
    }
  };
}

jsunity.run(optimizerRulesSuite);

return jsunity.done();
