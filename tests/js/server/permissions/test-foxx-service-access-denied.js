/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail */
'use strict';

if (getOptions === true) {
  let users = require("@arangodb/users");

  return {
    'server.harden': 'true',
    'server.authentication': 'true',
    'javascript.harden' : 'true',
    'javascript.files-black-list': [
      '^/etc/',
    ],
    'javascript.endpoints-white-list' : [
      'ssl://arangodb.com:443'
    ],
    'javascript.environment-variables-black-list': 'PATH',
    'javascript.startup-options-black-list': 'point|log',
  };
}

const jsunity = require('jsunity');
const fs = require('fs');
const internal = require('internal');
const db = internal.db;
const download = internal.download;
const arangodb = require('@arangodb');
const FoxxManager = require('@arangodb/foxx/manager');


function testSuite() {
  const basePath = fs.makeAbsolute(fs.join(require('internal').pathForTesting('common'), 'test-data', 'apps'));
  const foxxApp = fs.join(basePath, 'server-security');
  const mount = '/testmount';
  const collName = 'server_security';
  const prefix = mount.substr(1).replace(/[-.:/]/, '_');
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
    // js/common/test-data/apps/server-security/index.js

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
      assertTrue(body.startsWith("/tmp"));
    },

    testGetTmpFile : function() {
      const url = endpoint + mount + "/get-tmp-file";
      const res = download(url);
      assertEqual(200, res.code);
      let body = JSON.parse(res.body);
      assertTrue(body.startsWith("/tmp"));
    },

    testWriteTmpFile : function() {
      const url = endpoint + mount + "/write-tmp-file";
      const res = download(url);
      assertEqual(200, res.code);
    },

    testProcessStatistics : function() {
      const url = endpoint + mount + "/process-statistics";
      const res = download(url);
      assertEqual(403, res.code);
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


    //testTemplate : function() {
    //  const print = internal.print;
    //  return
    //  const url = endpoint + mount + "/bla";
    //  const res = download(url);
    //  print("UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUULLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLFF")
    //  assertEqual(200,200)
    //  assertEqual(403,403)
    //  print(res)
    //  if(res.code >= 200 && res.code < 300) {
    //    let body = JSON.parse(res.body)
    //    print(body)
    //  }
    //},


  };
}


jsunity.run(testSuite);
return jsunity.done();
