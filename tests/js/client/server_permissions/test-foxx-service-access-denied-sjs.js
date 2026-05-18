/*jshint globalstrict:false, strict:false */
/* global getOptions, fail */
'use strict';
const fs = require('fs');
const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const internal = require('internal');
const pu = require('@arangodb/testutils/process-utils');
const FoxxManager = require('@arangodb/foxx/manager');
let IM = global.instanceManager;

if (getOptions === true) {
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
      'ssl://arango.ai:443'
    ],
    'javascript.environment-variables-denylist': 'PATH',
    'javascript.startup-options-denylist': 'point|log',
  };
}

function testSuite() {
  const download = internal.download;
  const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
  const foxxApp = fs.join(basePath, 'server-security');
  const mount = '/testmount';

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
      const url = IM.url + mount + "/pid";
      const res = download(url);
      assertEqual(403, res.code);
      assertEqual("Forbidden", res.message);
    },

     testPasswd : function() {
       const url = IM.url + mount + "/passwd";
       const res = download(url);
       assertEqual(403, res.code);
       assertEqual("Forbidden", res.message);
     },

     testDlHeise : function() {
       const url = IM.url + mount + "/dl-heise";
       const res = download(url);
       assertEqual(403, res.code);
       assertEqual("Forbidden", res.message);
     },

     testTestPort : function() {
       const url = IM.url + mount + "/test-port";
       const res = download(url);
       assertEqual(403, res.code);
       assertEqual("Forbidden", res.message);
     },

     testGetTmpPath : function() {
       const url = IM.url + mount + "/get-tmp-path";
       const res = download(url);
       assertEqual(200, res.code);
       let body = JSON.parse(res.body);
     },

     testGetTmpFile : function() {
       const url = IM.url + mount + "/get-tmp-file";
       const res = download(url);
       assertEqual(200, res.code);
       let body = JSON.parse(res.body);
     },

     testWriteTmpFile : function() {
       const url = IM.url + mount + "/write-tmp-file";
       const res = download(url);
       assertEqual(200, res.code);
     },

     testProcessStatistics : function() {
       const url = IM.url + mount + "/process-statistics";
       const res = download(url);
       //disabled for oasis
       //assertEqual(403, res.code);
     },

     testExecuteExternal : function() {
       const url = IM.url + mount + "/execute-external";
       const res = download(url);
       assertEqual(403, res.code);
     },

     testPath : function() {
       { // read
         const url = IM.url + mount + "/environment-variables-get-path";
         const res = download(url);
         assertEqual(204, res.code);
         assertEqual("", res.body);
       }
       { // modify
         const url = IM.url + mount + "/environment-variables-set-path";
         const res = download(url);
         assertEqual(200, res.code);
         assertEqual("true", res.body);
       }
       { // read
         const url = IM.url + mount + "/environment-variables-get-path";
         const res = download(url);
         assertEqual(204, res.code);
         assertEqual("", res.body);
       }
     },

     testStartupOptions : function() {
       const url = IM.url + mount + "/startup-options-log-file";
       const res = download(url);
       assertEqual(204, res.code);
       assertEqual("", res.body);
     },

     testReadServiceFile : function() {
       const url = IM.url + mount + "/read-service-file";
       const res = download(url);
       assertEqual(200, res.code);
       let body = JSON.parse(res.body);
       assertTrue(body.startsWith("'use strict'"));
     },

     testWriteRemoveServiceFile : function() {
       {
         const url = IM.url + mount + "/write-service-file";
         const res = download(url);
         assertEqual(200, res.code);
       }
       {
         const url = IM.url + mount + "/remove-service-file";
         const res = download(url);
         assertEqual(200, res.code);
       }
     },

  };
}


jsunity.run(testSuite);
return jsunity.done();
