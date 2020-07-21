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
  const cn = 'UnitTestsDumpEdges';

  return {
    testDumpCompressed: function () {
      let tree = fs.listTree(dumpDir);
      assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
      let data = fs.readFileSync(fs.join(dumpDir, "ENCRYPTION")).toString();
      assertEqual("none", data);
      const prefix = "UnitTestsDumpEdges_8a31b923e9407ab76b6ca41131b8acf1";
      
      let structure = prefix + ".structure.json";
      if (!fs.isFile(fs.join(dumpDir, structure))) {
        structure = cn + ".structure.json";
      }
      let structureFile = fs.join(dumpDir, structure);
      assertTrue(fs.isFile(structureFile), "structure file does not exist: " + structureFile);
      assertNotEqual(-1, tree.indexOf(structure));
      data = JSON.parse(fs.readFileSync(fs.join(dumpDir, structure)).toString());
      assertEqual(cn, data.parameters.name);
      
      assertNotEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
      assertEqual(-1, tree.indexOf(prefix + ".data.json"));
      data = fs.readGzip(fs.join(dumpDir, prefix + ".data.json.gz")).toString().trim().split('\n');
      assertEqual(10, data.length);
      data.forEach(function(line) {
        line = JSON.parse(line);
        assertEqual(2300, line.type);
        assertTrue(line.data.hasOwnProperty('_key'));
        assertTrue(line.data.hasOwnProperty('_rev'));
      });
    }
  };
}

jsunity.run(dumpIntegrationSuite);

return jsunity.done();
