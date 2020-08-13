/*jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// / @author Lars Maier
// / @author Markus Pfeiffer
// / @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");

/* Run the "native" SSSP, the VertexAccumulators SSSP, and AQL ShortestPath and compare the
   results */
function exec_air_tests() {
  require("@arangodb/air/vertex-degrees").test();
  require("@arangodb/air/pagerank").test();
  require("@arangodb/air/single-source-shortest-paths").test();
  require("@arangodb/air/strongly-connected-components").test();
}

exec_air_tests();

// exports.exec_air_tests = exec_air_tests;

