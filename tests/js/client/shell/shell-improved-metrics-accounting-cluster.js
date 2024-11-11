/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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
/// @author Alexey Bakharew
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(internal.pathForTesting('client'), 
    'shell', 'shell-improved-metrics-accounting.inc');
const arangosearch_base = require("fs").join(internal.pathForTesting('client'),
    'shell', 'api', 'arangosearch-memory-metrics.inc');

const ImprovedMemoryAccounting = require("internal").load(base);
const ImprovedMemoryAccountingArangoSearch = require("internal").load(arangosearch_base);

let IM = global.instanceManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////
if (IM.debugCanUseFailAt()) {
  const base_fail_at = require("fs").join(internal.pathForTesting('client'),
    'shell', 'shell-improved-metrics-accounting-fail-at.inc');
    const ImprovedMemoryAccountingFailAt = internal.load(base_fail_at);

    jsunity.run(function ImprovedMemoryAccountingFailAtTestSuite_repl() {
        let suite = {};
        deriveTestSuite(
            ImprovedMemoryAccountingFailAt("ImprovedMemoryAccountingFailAtTestSuite_Repl", null, {
            replicationFactor:3,
            writeConcern:1,
            numberOfShards : 3}, false),
            suite,
            "_Repl"
        );
        return suite;
    });
}

jsunity.run(function ImprovedMemoryAccountingTestSuite_repl() {
    let suite = {};
    deriveTestSuite(
      ImprovedMemoryAccounting("ImprovedMemoryAccountingTestSuite_Repl", null, {
        replicationFactor:3,
        writeConcern:1,
        numberOfShards : 3}, false),
        suite,
        "_Repl"
    );
    return suite;
});

jsunity.run(function ImprovedMemoryAccountingArangoSearchTestSuite_repl() {
    let suite = {
    };

    deriveTestSuite(
      ImprovedMemoryAccountingArangoSearch("ImprovedMemoryAccountingArangoSearch_Repl", null, {
        replicationFactor:3,
        writeConcern:1,
        numberOfShards : 3}, false),
        suite,
        "_Repl"
    );
    return suite;
});

return jsunity.done();
