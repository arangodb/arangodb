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
const request = require('@arangodb/request');

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
  const foxxApp = fs.join(basePath, 'redirect');
  const mount = '/test-redirect';

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
      const res = request.get({
        url: IM.url + mount + "/redirectloop/0",
        followRedirects: true
      });
      print(res)
      assertEqual(403, res.code);
      assertEqual("403 Forbidden", res.errorMessage);
    },


  };
}


jsunity.run(testSuite);
return jsunity.done();
