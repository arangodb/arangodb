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
let internal = require('internal');
let arangodb = require('@arangodb');
let fs = require('fs');
let db = arangodb.db;

function dumpIntegrationSuite () {
  'use strict';
  const cn = 'UnitTestsDumpRestore';
  // TODO: find a way to detect the path of arangodump!!!!
  let binPath = fs.join('build', 'bin');
  const arangodump = fs.join(binPath, 'arangodump');

  assertTrue(fs.isFile(arangodump), "arangodump not found!");

  let addConnectionArgs = function(args) {
    args.push('--server.endpoint');
    args.push(arango.getEndpoint());
    args.push('--server.database');
    args.push(arango.getDatabaseName());
    args.push('--server.username');
    args.push(arango.connectedUser());
  };

  let runDump = function(path, args) {
    try {
      fs.removeDirectory(path);
    } catch (err) {}

    args.push('--output-directory');
    args.push(path);
    args.push('--collection');
    args.push(cn);
    addConnectionArgs(args);

    let rc = internal.executeExternalAndWait(arangodump, args);
    assertTrue(rc.hasOwnProperty("exit"));
    assertEqual(0, rc.exit);
    return fs.listTree(path);
  };

  return {

    setUp: function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 3 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testDumpCompressed: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'true'];
        let tree = runDump(path, args); 
        assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
        let data = fs.readFileSync(fs.join(path, "ENCRYPTION")).toString();
        assertEqual("none", data);
        const prefix = "UnitTestsDumpRestore_fa0ad5acf1c2ba4a5c04b4ddac6ee61e";

        assertNotEqual(-1, tree.indexOf(prefix + ".structure.json"));
        data = JSON.parse(fs.readFileSync(fs.join(path, prefix + ".structure.json")).toString());
        assertEqual(cn, data.parameters.name);
        
        assertNotEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
        assertEqual(-1, tree.indexOf(prefix + ".data.json"));
        data = fs.readGzip(fs.join(path, prefix + ".data.json.gz")).toString().trim().split('\n');
        assertEqual(1000, data.length);
        data.forEach(function(line) {
          line = JSON.parse(line);
          assertEqual(2300, line.type);
          assertTrue(line.data.hasOwnProperty('_key'));
          assertTrue(line.data.hasOwnProperty('_rev'));
        });
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpUncompressed: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'false'];
        let tree = runDump(path, args); 
        assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
        let data = fs.readFileSync(fs.join(path, "ENCRYPTION"));
        assertEqual("none", data.toString());
        
        const prefix = "UnitTestsDumpRestore_fa0ad5acf1c2ba4a5c04b4ddac6ee61e";

        assertNotEqual(-1, tree.indexOf(prefix + ".structure.json"));
        data = JSON.parse(fs.readFileSync(fs.join(path, prefix + ".structure.json")).toString());
        assertEqual(cn, data.parameters.name);
        
        assertEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
        assertNotEqual(-1, tree.indexOf(prefix + ".data.json"));
        data = fs.readFileSync(fs.join(path, prefix + ".data.json")).toString().trim().split('\n');
        assertEqual(1000, data.length);
        data.forEach(function(line) {
          line = JSON.parse(line);
          assertEqual(2300, line.type);
          assertTrue(line.data.hasOwnProperty('_key'));
          assertTrue(line.data.hasOwnProperty('_rev'));
        });
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },

    testDumpCompressedEncrypted: function () {
      if (!require("internal").isEnterprise()) {
        return;
      }

      let keyfile = fs.getTempFile();
      let path = fs.getTempFile();
      try {
        // 32 bytes of garbage
        fs.writeFileSync(keyfile, "01234567890123456789012345678901");

        let args = ['--compress-output', 'false', '--encryption.keyfile', keyfile];
        let tree = runDump(path, args); 
        assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
        let data = fs.readFileSync(fs.join(path, "ENCRYPTION")).toString();
        assertEqual("aes-256-ctr", data);
        const prefix = "UnitTestsDumpRestore_fa0ad5acf1c2ba4a5c04b4ddac6ee61e";
        assertNotEqual(-1, tree.indexOf(prefix + ".structure.json"));
        try {
          // cannot read encrypted file
          JSON.parse(fs.readFileSync(fs.join(path, prefix + ".structure.json")));
          fail();
        } catch (err) {
        }
        
        assertEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
        assertNotEqual(-1, tree.indexOf(prefix + ".data.json"));
        try {
          // cannot read encrypted file
          JSON.parse(fs.readFileSync(fs.join(path, prefix + ".data.json")));
          fail();
        } catch (err) {}
      } finally {
        try {
          fs.removeDirectory(path);
          fs.removeDirectory(keyfile);
        } catch (err) {}
      }
    },
    
    testDumpUncompressedEncrypted: function () {
      if (!require("internal").isEnterprise()) {
        return;
      }

      let keyfile = fs.getTempFile();
      let path = fs.getTempFile();
      try {
        // 32 bytes of garbage
        fs.writeFileSync(keyfile, "01234567890123456789012345678901");

        let args = ['--compress-output', 'true', '--encryption.keyfile', keyfile];
        let tree = runDump(path, args); 
        assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
        let data = fs.readFileSync(fs.join(path, "ENCRYPTION")).toString();
        assertEqual("aes-256-ctr", data);
        const prefix = "UnitTestsDumpRestore_fa0ad5acf1c2ba4a5c04b4ddac6ee61e";
        assertNotEqual(-1, tree.indexOf(prefix + ".structure.json"));
        try {
          // cannot read encrypted file
          JSON.parse(fs.readFileSync(fs.join(path, prefix + ".structure.json")));
          fail();
        } catch (err) {
        }
       
        // compression will turn itself off with encryption
        assertEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
        assertNotEqual(-1, tree.indexOf(prefix + ".data.json"));
        try {
          // cannot read encrypted file
          JSON.parse(fs.readFileSync(fs.join(path, prefix + ".data.json")));
          fail();
        } catch (err) {}
      } finally {
        try {
          fs.removeDirectory(path);
          fs.removeDirectory(keyfile);
        } catch (err) {}
      }
    },

  };
}

jsunity.run(dumpIntegrationSuite);

return jsunity.done();
