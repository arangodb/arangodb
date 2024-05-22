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
  // this file is used by multiple, hence the checked structure is only in a subdirectory:
  const dumpDir = process.env['dump-directory'];
  const cn = 'UnitTestsDumpEdges';
  const dbName = 'UnitTestsDumpSrc';

  return {
    testDumpUncompressed: function () {
      let ddir = dumpDir;
      if (fs.exists(fs.join(dumpDir, dbName))) {
        ddir = fs.join(dumpDir, dbName);
      }
      let tree = fs.listTree(ddir);
      assertNotEqual(-1, tree.indexOf("ENCRYPTION"), dumpDir);
      let data = fs.readFileSync(fs.join(dumpDir, "ENCRYPTION"));
      assertEqual("none", data.toString());
      
      const prefix = cn + "_8a31b923e9407ab76b6ca41131b8acf1";
      let structure = prefix + ".structure.json";
      let fullNameIndex = tree.indexOf(structure);
      if (fullNameIndex === -1) {
        structure = cn + ".structure.json";
        fullNameIndex = tree.indexOf(structure);
      }
      assertNotEqual(-1, fullNameIndex, `no ${fs.join(ddir, '*', structure)} in ${JSON.stringify(tree)}`);
      let structureFile = fs.join(ddir, tree[fullNameIndex]);

      assertTrue(fs.isFile(structureFile),"structure file does not exist: " + structureFile);
      assertNotEqual(-1, tree.indexOf(structure));
      data = JSON.parse(fs.readFileSync(fs.join(ddir, structure)).toString());
      assertEqual(cn, data.parameters.name);
      
      let files = tree.filter((f) =>
        (f.startsWith(prefix) || f.startsWith(fs.join(dbName, prefix))) &&
          f.match(/(\.\d+)?\.data\.json$/));
      assertNotEqual(0, files.length, files);
      files.forEach((file) => {
        data = fs.readFileSync(fs.join(ddir, file)).toString().trim().split('\n');
        assertEqual(10, data.length, fs.join(ddir, file));
        data.forEach(function(line) {
          line = JSON.parse(line);
          assertTrue(line.hasOwnProperty('_key'));
          assertTrue(line.hasOwnProperty('_rev'));
        });
      });
      
      files = tree.filter((f) => f.startsWith(prefix) && f.match(/(\.\d+)?\.data\.json\.gz$/));
      assertEqual(0, files.length, files);
    }
  };
}

jsunity.run(dumpIntegrationSuite);

return jsunity.done();
