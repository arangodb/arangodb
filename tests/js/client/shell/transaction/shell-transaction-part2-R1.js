/* jshint globalstrict:false, strict:false, maxlen: 200 */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
// / @author Alexandru Petenchea
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const fs = require("fs");
const { deriveTestSuite } = require("@arangodb/test-helper-common");
const { partSuites, transactionDatabaseSuite } = require("./" + fs.join("tests", "js", "client", "shell", "transaction", "shell-transaction.inc"));

for (let suite of partSuites[1]) {
  let derived = {};
  deriveTestSuite(suite({}), derived, "_V1");
  jsunity.run(function() {
    return derived;
  });
}
jsunity.run(transactionDatabaseSuite);

return jsunity.done();
