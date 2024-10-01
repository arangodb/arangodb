/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertFalse, arango */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  let args = {
    'javascript.copy-installation': 'true',
    'javascript.allow-admin-execute': 'true',
  };
  if (require("internal").platform === 'linux') {
    // turn off splicing. this is a linux-specific startup option
    args['use-splice-syscall'] =  'false';
  }
  return args;
}

const fs = require("fs");
const jsunity = require('jsunity');
const {
  getDbPath
} = require('@arangodb/test-helper');

function testSuite() {
  const errors = require('@arangodb').errors;

  return {
    testCanExecuteAction : function() {
      // fetch server-side database directory name
      let dbPath = getDbPath();
      let jsPath = fs.join(dbPath, "js");
      assertTrue(fs.exists(jsPath));
      assertTrue(fs.exists(fs.join(jsPath, "node")));
      assertTrue(fs.exists(fs.join(jsPath, "node", "node_modules")));
      assertTrue(fs.exists(fs.join(jsPath, "node", "node_modules", "lodash")));
      assertFalse(fs.exists(fs.join(jsPath, "node", "eslint")));
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
