/*jshint globalstrict:false, strict:false */
/* global getOptions, fail */
'use strict';
const fs = require('fs');
const jsunity = require("jsunity");
const { assertEqual, assertTrue, assertFalse, assertNotEqual, assertUndefined } = jsunity.jsUnity.assertions;
const internal = require('internal');
const pu = require('@arangodb/testutils/process-utils');
const FoxxManager = require('@arangodb/foxx/manager');
const arango = require('@arangodb').arango;
let IM = global.instanceManager;

if (getOptions === true) {
  let testPath = fs.join(pu.TOP_DIR, internal.pathForTesting(''));

  return {
    'temp.path': fs.getTempPath(), // Adjust the temp-path to match our current temp path
    'server.harden': 'true',
    'server.authentication': 'true',
    'server.jwt-secret': 'abc123',
    'javascript.harden': 'true',
    'javascript.files-allowlist': [
      '^' + fs.escapePath(testPath), // we need to call isDirectory (internal.pathForTesting) in
      // the server which is forbidden in not-allowed paths
    ],
    // tests/js/common/test-data/apps/server-security/index.js
    'javascript.app-path': fs.join(testPath, 'common', 'test-data', 'apps'),
    'javascript.endpoints-allowlist': [
      'ssl://arango.ai:443'
    ],
    'javascript.environment-variables-denylist': 'PATH',
    'javascript.startup-options-denylist': 'point|log',
  };
}

function testSuite() {
  const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
  const foxxApp = fs.join(basePath, 'server-security');
  const mount = '/testmount';

  return {
    setUp: function () {
      try {
        FoxxManager.uninstall(mount, { force: true });
        FoxxManager.install(foxxApp, mount);
      } catch (e) { }
    },

    tearDown: function () {
      try {
        FoxxManager.uninstall(mount, { force: false });
      } catch (e) {
      }
    },

    // routes are defined in:
    // tests/js/common/test-data/apps/server-security/index.js

    testPid: function () {
      const res = arango.GET_RAW(mount + "/pid");
      assertEqual(403, res.code);
      assertEqual("403 Forbidden", res.errorMessage);
    },

    testPasswd: function () {
      const res = arango.GET_RAW(mount + "/passwd");
      assertEqual(403, res.code);
      assertEqual("403 Forbidden", res.errorMessage);
    },

    testDlHeise: function () {
      const res = arango.GET_RAW(mount + "/dl-heise");
      assertEqual(403, res.code);
      assertEqual("403 Forbidden", res.errorMessage);
    },

    testTestPort: function () {
      const res = arango.GET_RAW(mount + "/test-port");
      assertEqual(403, res.code);
      assertEqual("403 Forbidden", res.errorMessage);
    },

    testGetTmpPath: function () {
      const res = arango.GET_RAW(mount + "/get-tmp-path");
      assertEqual(200, res.code);
    },

    testGetTmpFile: function () {
      const res = arango.GET_RAW(mount + "/get-tmp-file");
      assertEqual(200, res.code);
    },

    testWriteTmpFile: function () {
      const res = arango.GET_RAW(mount + "/write-tmp-file");
      assertEqual(200, res.code);
    },

    testProcessStatistics: function () {
      //const url = IM.url + mount + "/process-statistics";
      //disabled for oasis
      //assertEqual(403, res.code);
    },

    testExecuteExternal: function () {
      const res = arango.GET_RAW(mount + "/execute-external");
      assertEqual(403, res.code);
    },

    testPath: function () {
      { // read
        const res = arango.GET_RAW(mount + "/environment-variables-get-path");
        assertEqual(204, res.code);
        assertUndefined(res.parsedBody);
      }
      { // modify
        const res = arango.GET_RAW(mount + "/environment-variables-set-path");
        assertEqual(200, res.code);
        assertEqual("true", res.parsedBody);
      }
      { // read
        const res = arango.GET_RAW(mount + "/environment-variables-get-path");
        assertEqual(204, res.code);
        assertUndefined(res.parsedBody);
      }
    },

    testStartupOptions: function () {
      const res = arango.GET_RAW(mount + "/startup-options-log-file");
      assertEqual(204, res.code);
      assertUndefined(res.parsedBody);
    },

    testReadServiceFile: function () {
      const res = arango.GET_RAW(mount + "/read-service-file");
      assertEqual(200, res.code);
      assertTrue(res.parsedBody.startsWith("'use strict'"));
    },

    testWriteRemoveServiceFile: function () {
      {
        const res = arango.GET_RAW(mount + "/write-service-file");
        assertEqual(200, res.code);
      }
      {
        const res = arango.GET_RAW(mount + "/remove-service-file");
        assertEqual(200, res.code);
      }
    },

  };
}


jsunity.run(testSuite);
return jsunity.done();
