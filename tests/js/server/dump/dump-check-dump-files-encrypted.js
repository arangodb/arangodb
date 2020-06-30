/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let fs = require('fs');

function dumpIntegrationSuite () {
  'use strict';
  const dumpDir = process.env['dump-directory'];
  const cn = 'UnitTestsDumpMany';

  return {
    testDumpCompressedEncrypted: function () {
      let tree = fs.listTree(dumpDir);
      assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
      let data = fs.readFileSync(fs.join(dumpDir, "ENCRYPTION")).toString();
      assertEqual("aes-256-ctr", data);
      const prefix = "UnitTestsDumpMany_13150d0bdc1db4344e331450dbe6cbde";

      let structure = prefix + ".structure.json";
      if (!fs.isFile(fs.join(dumpDir, structure))) {
        structure = cn + ".structure.json";
      }
      assertTrue(fs.isFile(fs.join(dumpDir, structure)), structure);
      assertNotEqual(-1, tree.indexOf(structure));
      try {
        // cannot read encrypted file
        JSON.parse(fs.readFileSync(fs.join(dumpDir, structure)));
        fail();
      } catch (err) {
        assertTrue(err instanceof SyntaxError, err);
      }
      
      assertEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
      assertNotEqual(-1, tree.indexOf(prefix + ".data.json"));
      try {
        // cannot read encrypted file
        JSON.parse(fs.readFileSync(fs.join(dumpDir, prefix + ".data.json")));
        fail();
      } catch (err) {
        assertTrue(err instanceof SyntaxError, err);
      }
    }
  };
}

jsunity.run(dumpIntegrationSuite);

return jsunity.done();
