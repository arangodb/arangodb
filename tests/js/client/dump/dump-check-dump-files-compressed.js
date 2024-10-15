/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertEqual, assertNotEqual, arango */

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
      assertNotEqual(-1, tree.indexOf("ENCRYPTION"), dumpDir);
      let data = fs.readFileSync(fs.join(dumpDir, "ENCRYPTION")).toString();
      assertEqual("none", data);
      const prefix = "UnitTestsDumpEdges_8a31b923e9407ab76b6ca41131b8acf1";
      
      let structure = prefix + ".structure.json";
      if (!fs.isFile(fs.join(dumpDir, structure))) {
        structure = cn + ".structure.json";
      }
      if (!fs.isFile(fs.join(dumpDir, structure))) {
        structure = prefix + ".structure.vpack";
      }
      if (!fs.isFile(fs.join(dumpDir, structure))) {
        structure = cn + ".structure.vpack";
      }
      let structureFile = fs.join(dumpDir, structure);
      assertTrue(fs.isFile(structureFile), "structure file does not exist: " + structureFile);
      assertNotEqual(-1, tree.indexOf(structure));
      data = JSON.parse(fs.readFileSync(fs.join(dumpDir, structure)).toString());
      assertEqual(cn, data.parameters.name);
      let files = tree.filter((f) => f.startsWith(prefix) && f.match(/(\.\d+)?\.data\.[jsonvpack]*\.gz$/));
      assertNotEqual(0, files.length, files);
      files.forEach((file) => {
        if (file.search('vpack')>0) {
          // TODO: VPACK_TO_V8 can't handle this
          // data = fs.readGzip(fs.join(dumpDir, file));
          // print(VPACK_TO_V8(data))
          //print(data) // .toString().trim().split('\n');
        } else {
          data = fs.readGzip(fs.join(dumpDir, file)).toString().trim().split('\n');
          assertEqual(10, data.length);
          data.forEach(function(line) {
            line = JSON.parse(line);
            assertTrue(line.hasOwnProperty('_key'));
            assertTrue(line.hasOwnProperty('_rev'));
          });
        }
      });
      
      files = tree.filter((f) => f.startsWith(prefix) && f.match(/(\.\d+)?\.data\.j[jsonvpack]*$/));
      assertEqual(0, files.length, files);
    }
  };
}

jsunity.run(dumpIntegrationSuite);

return jsunity.done();
