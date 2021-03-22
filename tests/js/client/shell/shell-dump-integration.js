/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, arango */

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
let pu = require('@arangodb/testutils/process-utils');
let db = arangodb.db;
let isCluster = require("internal").isCluster();

function dumpIntegrationSuite () {
  'use strict';
  const cn = 'UnitTestsDump';
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

  let structureFile = function(path, cn) {
    const prefix = cn + "_" + require("@arangodb/crypto").md5(cn);
    let structure = prefix + ".structure.json";
    if (!fs.isFile(fs.join(path, structure))) {
      // seems necessary in cluster
      structure = cn + ".structure.json";
    }
    return structure;
  };

  let checkStructureFile = function(tree, path, readable, cn) {
    let structure = structureFile(path, cn);
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

  let checkDataFile = function(tree, path, compressed, envelopes, readable, cn) {
    const prefix = cn + "_" + require("@arangodb/crypto").md5(cn);
    let checkData = function(data, envelopes) {
      assertEqual(1000, data.length);
      data.forEach(function(line) {
        line = JSON.parse(line);
        if (envelopes) {
          assertEqual(2300, line.type);
          assertTrue(line.hasOwnProperty('data'));
          assertTrue(line.data.hasOwnProperty('_key'));
          assertTrue(line.data.hasOwnProperty('_rev'));
        } else {
          assertFalse(line.hasOwnProperty('type'));
          assertFalse(line.hasOwnProperty('data'));
          assertTrue(line.hasOwnProperty('_key'));
          assertTrue(line.hasOwnProperty('_rev'));
        }
      });
    };

    if (compressed) {
      assertTrue(readable);

      assertNotEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
      assertEqual(-1, tree.indexOf(prefix + ".data.json"));
      
      let data = fs.readGzip(fs.join(path, prefix + ".data.json.gz")).toString().trim().split('\n');
      checkData(data, envelopes);
    } else {
      assertEqual(-1, tree.indexOf(prefix + ".data.json.gz"));
      assertNotEqual(-1, tree.indexOf(prefix + ".data.json"));
      
      if (readable) {
        let data = fs.readFileSync(fs.join(path, prefix + ".data.json")).toString().trim().split('\n');
        checkData(data, envelopes);
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
      
      db._drop(cn + "Other");
      c = db._create(cn + "Other", { numberOfShards: 3 });
      c.insert(docs);
      
      db._drop(cn + "Padded");
      c = db._create(cn + "Padded", { keyOptions: { type: "padded" }, numberOfShards: 3 });
      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({});
      }
      c.insert(docs);
      
      db._drop(cn + "AutoIncrement");
      if (!isCluster) {
        c = db._create(cn + "AutoIncrement", { keyOptions: { type: "autoincrement" }, numberOfShards: 3 });
        docs = [];
        for (let i = 0; i < 1000; ++i) {
          docs.push({});
        }
        c.insert(docs);
      }
    },

    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn + "Other");
      db._drop(cn + "Padded");
      db._drop(cn + "AutoIncrement");
    },
    
    testDumpOnlyOneShard: function () {
      if (!isCluster) {
        return;
      }

      let path = fs.getTempFile();

      let c = db._collection(cn);
      let shardCounts = c.count(true);
      let shards = Object.keys(shardCounts);

      assertEqual(3, shards.length);
      try {
        let args = ['--collection', cn, '--dump-data', 'true', '--compress-output', 'false', '--shard', shards[0]];
        let tree = runDump(path, args, 0); 
   
        const prefix = cn + "_" + require("@arangodb/crypto").md5(cn);
        let file = fs.join(path, prefix + '.data.json');
        let data = fs.readFileSync(file).toString();
        assertEqual(shardCounts[shards[0]] + 1, data.split('\n').length);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOnlyTwoShards: function () {
      if (!isCluster) {
        return;
      }

      let path = fs.getTempFile();

      let c = db._collection(cn);
      let shardCounts = c.count(true);
      let shards = Object.keys(shardCounts);

      assertEqual(3, shards.length);
      try {
        let args = ['--collection', cn, '--dump-data', 'true', '--compress-output', 'false', '--shard', shards[0], '--shard', shards[1]];
        let tree = runDump(path, args, 0); 
   
        const prefix = cn + "_" + require("@arangodb/crypto").md5(cn);
        let file = fs.join(path, prefix + '.data.json');
        let data = fs.readFileSync(file).toString();
        assertEqual(shardCounts[shards[0]] + shardCounts[shards[1]] + 1, data.split('\n').length);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
   
    testDumpAutoIncrementKeyGenerator: function () {
      if (isCluster) {
        // autoincrement key generator is not supported in cluster
        return;
      }

      let path = fs.getTempFile();
      try {
        let args = ['--collection', cn + 'AutoIncrement', '--dump-data', 'false'];
        let tree = runDump(path, args, 0); 
        checkStructureFile(tree, path, true, cn + 'AutoIncrement');
        let structure = structureFile(path, cn + 'AutoIncrement');
        let data = JSON.parse(fs.readFileSync(fs.join(path, structure)).toString());
        assertEqual("autoincrement", data.parameters.keyOptions.type);
        let c = db._collection(cn + 'AutoIncrement');
        assertEqual(1000, c.count());
        let p = c.properties();
        let lastValue = p.keyOptions.lastValue;
        assertTrue(lastValue > 0, lastValue);
        assertEqual(lastValue, data.parameters.keyOptions.lastValue);

      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpPaddedKeyGenerator: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--collection', cn + 'Padded', '--dump-data', 'false'];
        let tree = runDump(path, args, 0); 
        checkStructureFile(tree, path, true, cn + 'Padded');
        let structure = structureFile(path, cn + 'Padded');
        let data = JSON.parse(fs.readFileSync(fs.join(path, structure)).toString());
        assertEqual("padded", data.parameters.keyOptions.type);
        let c = db._collection(cn + 'Padded');
        assertEqual(1000, c.count());
        let p = c.properties();
        let lastValue = p.keyOptions.lastValue;
        assertTrue(lastValue > 0, lastValue);
        assertEqual(lastValue, data.parameters.keyOptions.lastValue);

      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpCompressedEncryptedNoEnvelope: function () {
      if (!require("internal").isEnterprise()) {
        return;
      }

      let keyfile = fs.getTempFile();
      let path = fs.getTempFile();
      try {
        // 32 bytes of garbage
        fs.writeFileSync(keyfile, "01234567890123456789012345678901");

        let args = ['--compress-output', 'true', '--envelope', 'false', '--encryption.keyfile', keyfile, '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "aes-256-ctr");
        checkStructureFile(tree, path, false, cn);
        checkDataFile(tree, path, false, false, false, cn);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteUncompressedNoEnvelope: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'false', '--envelope', 'true', '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, false, true, true, cn);
        
        // second dump, which overwrites
        args = ['--compress-output', 'false', '--envelope', 'false', '--overwrite', 'true', '--collection', cn];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, false, false, true, cn);
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

        let args = ['--compress-output', 'true', '--encryption.keyfile', keyfile, '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "aes-256-ctr");
        checkStructureFile(tree, path, false, cn);
        checkDataFile(tree, path, false, true, false, cn);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteDisabled: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--collection', cn];
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
        let args = ['--compress-output', 'true', '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, true, true, true, cn);
        
        // second dump, which overwrites
        args = ['--compress-output', 'true', '--overwrite', 'true', '--collection', cn];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, true, true, true, cn);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteUncompressed: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'false', '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, false, true, true, cn);
        
        // second dump, which overwrites
        args = ['--compress-output', 'false', '--overwrite', 'true', '--collection', cn];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, false, true, true, cn);
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

        let args = ['--compress-output', 'false', '--encryption.keyfile', keyfile, '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "aes-256-ctr");
        checkStructureFile(tree, path, false, cn);
        checkDataFile(tree, path, false, true, false, cn);

        // second dump, which overwrites
        args = ['--compress-output', 'false', '--encryption.keyfile', keyfile, '--overwrite', 'true', '--collection', cn];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "aes-256-ctr");
        checkStructureFile(tree, path, false, cn);
        checkDataFile(tree, path, false, true, false, cn);
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

        let args = ['--compress-output', 'true', '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, true, true, true, cn);
        
        // second dump, which overwrites
        // this is expected to have an exit code of 1
        args = ['--compress-output', 'false', '--encryption.keyfile', keyfile, '--overwrite', 'true', '--collection', cn];
        runDump(path, args, 1 /*exit code*/);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOverwriteOtherCompressed: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'true', '--collection', cn];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, true, true, true, cn);
        
        // second dump, which overwrites
        args = ['--compress-output', 'true', '--overwrite', 'true', '--collection', cn + "Other"];
        tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, true, true, true, cn);
        checkStructureFile(tree, path, true, cn + "Other");
        checkDataFile(tree, path, true, true, true, cn + "Other");
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpJustOneInvalidCollection: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--collection', 'foobarbaz'];
        let tree = runDump(path, args, 1);
        checkEncryption(tree, path, "none");
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpJustInvalidCollections: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--collection', 'foobarbaz', '--collection', 'knarzknarzknarz'];
        let tree = runDump(path, args, 1); 
        checkEncryption(tree, path, "none");
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
    testDumpOneValidCollection: function () {
      let path = fs.getTempFile();
      try {
        let args = ['--compress-output', 'true', '--collection', cn, '--collection', 'knarzknarzknarz'];
        let tree = runDump(path, args, 0); 
        checkEncryption(tree, path, "none");
        checkStructureFile(tree, path, true, cn);
        checkDataFile(tree, path, true, true, true, cn);
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
