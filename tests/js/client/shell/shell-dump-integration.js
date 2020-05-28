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
let pu = require('@arangodb/process-utils');
let db = arangodb.db;

function dumpIntegrationSuite () {
  'use strict';
  const cn = 'UnitTestsDump';
  const prefix = "UnitTestsDump_4b80e603f9e6b0c22a37da945ed6a9c4"; 
  // detect the path of arangodump. quite hacky, but works
  const arangodump = fs.join(global.ARANGOSH_PATH, 'arangodump' + pu.executableExt);

  assertTrue(fs.isFile(arangodump), "arangodump not found!");

  let addConnectionArgs = function(args) {
    let endpoint = arango.getEndpoint().replace(/\+vpp/, '').replace(/^http:/, 'tcp:').replace(/^https:/, 'ssl:').replace(/^vst:/, 'tcp:').replace(/^h2:/, 'tcp:');
    args.push('--server.endpoint');
    args.push(endpoint);
    args.push('--server.database');
    args.push(arango.getDatabaseName());
    args.push('--server.username');
    args.push(arango.connectedUser());
  };

  let runDump = function(path, args, rc) {
    try {
      fs.removeDirectory(path);
    } catch (err) {}

    args.push('--output-directory');
    args.push(path);
    args.push('--collection');
    args.push(cn);
    addConnectionArgs(args);

    let actualRc = internal.executeExternalAndWait(arangodump, args);
    assertTrue(actualRc.hasOwnProperty("exit"));
    assertEqual(rc, actualRc.exit);
    return fs.listTree(path);
  };

  let checkEncryption = function(tree, path, expected) {
    assertNotEqual(-1, tree.indexOf("ENCRYPTION"));
    let data = fs.readFileSync(fs.join(path, "ENCRYPTION")).toString();
    assertEqual(expected, data);
  };

  let checkStructureFile = function(tree, path, readable) {
    let structure = prefix + ".structure.json";
    if (!fs.isFile(fs.join(path, structure))) {
      // seems necessary in cluster
      structure = cn + ".structure.json";
    }
    assertTrue(fs.isFile(fs.join(path, structure)), structure);
    assertNotEqual(-1, tree.indexOf(structure));

    if (readable) {
      let data = JSON.parse(fs.readFileSync(fs.join(path, structure)).toString());
      assertEqual(cn, data.parameters.name);
    } else {
      try {
        // cannot read encrypted file
        JSON.parse(fs.readFileSync(fs.join(path, structure)));
        fail();
      } catch (err) {
        // error is expected here
        assertTrue(err instanceof SyntaxError, err);
      }
    }
  };

  let checkDataFile = function(tree, path, compressed, readable) {
    if (compressed) {
      assertTrue(readable);

      assertNotEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
      assertEqual(-1, tree.indexOf(prefix + ".data.json"));
      
      let data = fs.readGzip(fs.join(path, prefix + ".data.json.gz")).toString().trim().split('\n');
      assertEqual(1000, data.length);
      data.forEach(function(line) {
        line = JSON.parse(line);
        assertEqual(2300, line.type);
        assertTrue(line.data.hasOwnProperty('_key'));
        assertTrue(line.data.hasOwnProperty('_rev'));
      });
    } else {
      assertEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
      assertNotEqual(-1, tree.indexOf(prefix + ".data.json"));
      
      if (readable) {
        let data = fs.readFileSync(fs.join(path, prefix + ".data.json")).toString().trim().split('\n');
        assertEqual(1000, data.length);
        data.forEach(function(line) {
          line = JSON.parse(line);
          assertEqual(2300, line.type);
          assertTrue(line.data.hasOwnProperty('_key'));
          assertTrue(line.data.hasOwnProperty('_rev'));
        });
      } else {
        try {
          // cannot read encrypted file
          JSON.parse(fs.readFileSync(fs.join(path, prefix + ".data.json")));
          fail();
        } catch (err) {
          // error is expected here
          assertTrue(err instanceof SyntaxError, err);
        }
      }
    }
  };

  return {

    setUpAll: function () {
      db._drop(cn);
      let c = db._create(cn, { numberOfShards: 3 });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(cn);
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

        let args = ['--compress-output', 'true', '--encryption.keyfile', keyfile];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "aes-256-ctr");
        checkStructureFile(tree, path, false);
        checkDataFile(tree, path, false, false);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteDisabled: function () {
      let path = fs.getTempFile();
      try {
        let args = [];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
       
        // second dump, without overwrite
        // this is expected to have an exit code of 1
        runDump(path, args, 1 /*exit code*/);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },

    testDumpOverwriteCompressed: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'true'];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true);
        checkDataFile(tree, path, true, true);
        
        // second dump, which overwrites
        args = ['--compress-output', 'true', '--overwrite', 'true'];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true);
        checkDataFile(tree, path, true, true);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteUncompressed: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'false'];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true);
        checkDataFile(tree, path, false, true);
        
        // second dump, which overwrites
        args = ['--compress-output', 'false', '--overwrite', 'true'];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true);
        checkDataFile(tree, path, false, true);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteEncrypted: function () {
      if (!require("internal").isEnterprise()) {
        return;
      }

      let keyfile = fs.getTempFile();
      let path = fs.getTempFile();
      try {
        // 32 bytes of garbage
        fs.writeFileSync(keyfile, "01234567890123456789012345678901");

        let args = ['--compress-output', 'false', '--encryption.keyfile', keyfile];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "aes-256-ctr");
        checkStructureFile(tree, path, false);
        checkDataFile(tree, path, false, false);

        // second dump, which overwrites
        args = ['--compress-output', 'false', '--encryption.keyfile', keyfile, '--overwrite', 'true'];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "aes-256-ctr");
        checkStructureFile(tree, path, false);
        checkDataFile(tree, path, false, false);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteCompressedWithEncrypted: function () {
      if (!require("internal").isEnterprise()) {
        return;
      }

      let keyfile = fs.getTempFile();
      let path = fs.getTempFile();
      try {
        // 32 bytes of garbage
        fs.writeFileSync(keyfile, "01234567890123456789012345678901");

        let args = ['--compress-output', 'true'];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true);
        checkDataFile(tree, path, true, true);
        
        // second dump, which overwrites
        // this is expected to have an exit code of 1
        args = ['--compress-output', 'false', '--encryption.keyfile', keyfile, '--overwrite', 'true'];
        runDump(path, args, 1 /*exit code*/);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
  };
}

jsunity.run(dumpIntegrationSuite);

return jsunity.done();
