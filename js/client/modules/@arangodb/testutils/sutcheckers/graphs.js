/* jshint strict: false, sub: true */
/* global print db arango */
'use strict';

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks that no new graphs were left on the SUT. 
// //////////////////////////////////////////////////////////////////////////////
exports.checker = class {
  constructor(runner) {
    this.runner = runner;
    this.name = 'graphs';
    this.graphCount = 0;
  }
  setUp(te) {
    this.graphCount = db._collection('_graphs').count();
    return true;
  }
  runCheck(te) {
    let graphs;
    try {
      graphs = db._collection('_graphs');
    } catch (x) {
      this.runner.setResult(te, false, {
        status: false,
        message: 'failed to fetch the graphs: ' + x.message
      });
      return false;
    }
    if (graphs && graphs.count() !== this.graphCount) {
      this.runner.setResult(te, false, {
        status: false,
        message: 'Cleanup of graphs missing - found graph definitions: ' +
          JSON.stringify(graphs.toArray())
      });
      this.graphCount = graphs.count();
      return false;
    }
    return true;
  }
};
