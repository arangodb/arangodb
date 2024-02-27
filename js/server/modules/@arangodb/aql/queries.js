/* global AQL_QUERIES_CLEAR_SLOW, AQL_QUERIES_SLOW, AQL_QUERIES_CURRENT, AQL_QUERIES_PROPERTIES,
  AQL_QUERIES_KILL */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// / @brief clears the slow query log
exports.clearSlow = function (params) {
  'use strict';

  return AQL_QUERIES_CLEAR_SLOW(params);
};

// / @brief returns the slow queries
exports.slow = function (params) {
  'use strict';

  return AQL_QUERIES_SLOW(params);
};

// / @brief returns the current queries
exports.current = function (params) {
  'use strict';

  return AQL_QUERIES_CURRENT(params);
};

// / @brief configures the query tracking properties
exports.properties = function (config) {
  'use strict';

  if (config === undefined) {
    return AQL_QUERIES_PROPERTIES();
  }
  return AQL_QUERIES_PROPERTIES(config);
};

// / @brief kills a query
exports.kill = function (params) {
  'use strict';
  
  return AQL_QUERIES_KILL(params);
};
