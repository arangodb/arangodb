/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, fail */
'use strict';
const fs = require('fs');
const internal = require('internal');
const pu = require('@arangodb/testutils/process-utils');

if (getOptions === true) {
  let users = require("@arangodb/users");
  let testPath = fs.join(pu.TOP_DIR, internal.pathForTesting(''));

  return {
    'temp.path': fs.getTempPath(), // Adjust the temp-path to match our current temp path
    'server.harden': 'true',
    'server.authentication': 'true',
    'server.jwt-secret': 'abc123',
    'javascript.harden' : 'true',
    'javascript.files-allowlist': [
      '^' + fs.escapePath(testPath), // we need to call isDirectory (internal.pathForTesting) in
                        // the server which is forbidden in not-allowed paths
    ],
    // tests/js/common/test-data/apps/server-security/index.js
    'javascript.app-path': fs.join(testPath, 'common', 'test-data', 'apps'),
    'javascript.endpoints-allowlist' : [
      'ssl://arangodb.com:443'
    ],
    'javascript.environment-variables-denylist': 'PATH',
    'javascript.startup-options-denylist': 'point|log',
  };
}


const jsunity = require('jsunity');

require("@arangodb/test-helper").waitForFoxxInitialized();

function testSuite() {
  const db = internal.db;
  const arangodb = require('@arangodb');
  const FoxxManager = require('@arangodb/foxx/manager');
  const download = internal.download;
  const basePath = fs.makeAbsolute(fs.join(require('internal').pathForTesting('common'), 'test-data', 'apps'));
  const foxxApp = fs.join(basePath, 'server-security');
  const mount = '/testmount';
  const endpoint = arangodb.arango.getEndpoint().replace('tcp://', 'http://');

  return {
    setUp: function() {
      try {
        FoxxManager.uninstall(mount, {force: true});
        FoxxManager.install(foxxApp, mount);
      } catch (e) { }
    },

    tearDown: function() {
      try {
        FoxxManager.uninstall(mount, {force: false});
      } catch (e) {
      }
    },

    // routes are defined in:
    // tests/js/common/test-data/apps/server-security/index.js

    testPid : function() {
      const url = endpoint + mount + "/pid";
      const res = download(url);
      assertEqual(403, res.code);
      assertEqual("Forbidden", res.message);
    },

     testPasswd : function() {
       const url = endpoint + mount + "/passwd";
       const res = download(url);
       assertEqual(403, res.code);
       assertEqual("Forbidden", res.message);
     },

     testDlHeise : function() {
       const url = endpoint + mount + "/dl-heise";
       const res = download(url);
       assertEqual(403, res.code);
       assertEqual("Forbidden", res.message);
     },

     testTestPort : function() {
       const url = endpoint + mount + "/test-port";
       const res = download(url);
       assertEqual(403, res.code);
       assertEqual("Forbidden", res.message);
     },

     testGetTmpPath : function() {
       const url = endpoint + mount + "/get-tmp-path";
       const res = download(url);
       assertEqual(200, res.code);
       let body = JSON.parse(res.body);
     },

     testGetTmpFile : function() {
       const url = endpoint + mount + "/get-tmp-file";
       const res = download(url);
       assertEqual(200, res.code);
       let body = JSON.parse(res.body);
     },

     testWriteTmpFile : function() {
       const url = endpoint + mount + "/write-tmp-file";
       const res = download(url);
       assertEqual(200, res.code);
     },

     testProcessStatistics : function() {
       const url = endpoint + mount + "/process-statistics";
       const res = download(url);
       //disabled for oasis
       //assertEqual(403, res.code);
     },

     testExecuteExternal : function() {
       const url = endpoint + mount + "/execute-external";
       const res = download(url);
       assertEqual(403, res.code);
     },

     testPath : function() {
       { // read
         const url = endpoint + mount + "/environment-variables-get-path";
         const res = download(url);
         assertEqual(204, res.code);
         assertEqual("undefined", res.body);
       }
       { // modify
         const url = endpoint + mount + "/environment-variables-set-path";
         const res = download(url);
         assertEqual(200, res.code);
         assertEqual("true", res.body);
       }
       { // read
         const url = endpoint + mount + "/environment-variables-get-path";
         const res = download(url);
         assertEqual(204, res.code);
         assertEqual("undefined", res.body);
       }
     },

     testStartupOptions : function() {
       const url = endpoint + mount + "/startup-options-log-file";
       const res = download(url);
       assertEqual(204, res.code);
       assertEqual("undefined", res.body);
     },

     testReadServiceFile : function() {
       const url = endpoint + mount + "/read-service-file";
       const res = download(url);
       assertEqual(200, res.code);
       let body = JSON.parse(res.body);
       assertTrue(body.startsWith("'use strict'"));
     },

     testWriteRemoveServiceFile : function() {
       {
         const url = endpoint + mount + "/write-service-file";
         const res = download(url);
         assertEqual(200, res.code);
       }
       {
         const url = endpoint + mount + "/remove-service-file";
         const res = download(url);
         assertEqual(200, res.code);
       }
     },

  };
}


jsunity.run(testSuite);
return jsunity.done();
