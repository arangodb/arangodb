/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let fs = require('fs');

function dumpIntegrationSuite() {
  'use strict';
  // this file is used by multiple, hence the checked structure is only in a subdirectory:
  const dumpDir = fs.join(process.env['dump-directory'], 'UnitTestsDumpSrc');
  const cn = 'UnitTestComputedValues';
  const prefix = cn + "_" + require("@arangodb/crypto").md5(cn);

  let checkValues = function(data) {
    data.forEach(function(line) {
      line = JSON.parse(line);
      assertEqual(2300, line.type);
      assertTrue(line.data.hasOwnProperty('_key'));
      assertTrue(line.data.hasOwnProperty('_rev'));
      assertTrue(line.data.hasOwnProperty('value1'));
      assertTrue(line.data.hasOwnProperty('value2'));
      assertTrue(line.data.hasOwnProperty('value3'));
      assertTrue(line.data.hasOwnProperty('value4'));
      assertEqual(line.value3, line.value1 + "+" + line.value2);
      assertEqual(line.value4, line.value2 + " " + line.value1);
    });
  };

  return {
    testDumpUncompressedComputedValues: function() {
      let tree = fs.listTree(dumpDir);
      assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
      let data = fs.readFileSync(fs.join(dumpDir, "ENCRYPTION"));
      assertEqual("none", data.toString());

      let structure = prefix + ".structure.json";
      if (!fs.isFile(fs.join(dumpDir, structure))) {
        structure = cn + ".structure.json";
      }

      let structureFile = fs.join(dumpDir, structure);
      assertTrue(fs.isFile(structureFile), "structure file does not exist: " + structureFile);
      assertNotEqual(-1, tree.indexOf(structure));
      data = JSON.parse(fs.readFileSync(fs.join(dumpDir, structure)).toString());
      assertEqual(cn, data.parameters.name);

      assertEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
      assertNotEqual(-1, tree.indexOf(prefix + ".data.json"));
      data = fs.readFileSync(fs.join(dumpDir, prefix + ".data.json")).toString().trim().split('\n');
      assertEqual(10, data.length);
      checkValues(data);
    },

    testDumpCompressedComputedValues: function() {
      let tree = fs.listTree(dumpDir);
      assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
      let data = fs.readFileSync(fs.join(dumpDir, "ENCRYPTION")).toString();
      assertEqual("none", data);

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
      checkValues(data);
    }
  };
}

jsunity.run(dumpIntegrationSuite);

return jsunity.done();
