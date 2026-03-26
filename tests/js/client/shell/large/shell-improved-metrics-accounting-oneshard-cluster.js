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
const ImprovedMemoryAccounting = internal.load(base);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

if (global.instanceManager.debugCanUseFailAt()) {
    const base_fail_at = require("fs").join(internal.pathForTesting('client'), 
    'shell', 'shell-improved-metrics-accounting-fail-at.inc');
    const ImprovedMemoryAccountingFailAt = internal.load(base_fail_at);

    jsunity.run(function ImprovedMemoryAccountingFailAtTestSuite_oneshard() {
        let suite = {};
        deriveTestSuite(
            ImprovedMemoryAccountingFailAt("InvertedIndexSearchAliasqlTestSuite_OneShard", {sharding: "single"}, {}),
            suite,
            "_OneShard"
        );
        return suite;
    });
}

jsunity.run(function ImprovedMemoryAccountingTestSuite_oneshard() {
    let suite = {};
    deriveTestSuite(
      ImprovedMemoryAccounting("ImprovedMemoryAccountingTestSuite_OneShard", {sharding: "single"}, {}),
        suite,
        "_OneShard"
    );
    return suite;
});

return jsunity.done();
