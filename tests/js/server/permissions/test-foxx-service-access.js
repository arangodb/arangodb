/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */
'use strict';

if (getOptions === true) {
  let users = require("@arangodb/users");

  //users.save("test_rw", "testi");
  //users.grantDatabase("test_rw", "_system", "rw");
  //users.save("test_ro", "testi");
  //users.grantDatabase("test_ro", "_system", "ro");

  return {
    'server.harden': 'false',
    'server.authentication': 'true',
    'javascript.harden' : 'true',
    'javascript.files-black-list': [
      '^/etc/',
    ],
    'javascript.endpoints-white-list' : [
      'ssl://arangodb.com:443'
    ],
  };
}

const jsunity = require('jsunity');

const FoxxManager = require('@arangodb/foxx/manager');
const fs = require('fs');

const internal = require('internal');
const db = internal.db;
const download = internal.download;

const arangodb = require('@arangodb');
const arango = arangodb.arango;
const aql = arangodb.aql;
const ArangoCollection = arangodb.ArangoCollection;

const basePath = fs.makeAbsolute(fs.join(require('internal').pathForTesting('common'), 'test-data', 'apps'));

const print = internal.print;









function testSuite() {
  const foxxApp = fs.join(basePath, 'server-security');
  const mount = '/testmount';
  const collName = 'server_security';
  const prefix = mount.substr(1).replace(/[-.:/]/, '_');
  const endpoint = arango.getEndpoint().replace('tcp://', 'http://');

  return {
    setUp: function() {
      try {
        FoxxManager.uninstall(mount, {force: true});
        FoxxManager.install(foxxApp, mount);
      } catch (e) {
      }

      try {
        db._drop(collName);
      } catch (e) {
      }
    },

    tearDown: function() {
      try {
        FoxxManager.uninstall(mount, {force: false});
      } catch (e) {
      }

      try {
        db._drop(collName);
      } catch (e) {
      }

      try {
        // some tests create these:
        db._drop(collName);
      } catch (e) {
      }
    },


    testServiceInstalled : function() {
      const checksum = db._query(aql`
        FOR service IN _apps
        FILTER service.mount == ${mount}
        RETURN service.checksum
      `).next();
      assertTrue(typeof checksum === 'string');
      assertTrue(checksum !== '');
    },

    testTest : function() {
          const url = endpoint + mount + "/test";
          const res = download(url);
          assertTrue(res.body === "true");
    },

    testPid : function() {
          const url = endpoint + mount + "/pid";
          const res = download(url);
          assertTrue(res.code === 403);
          assertTrue(res.message === "Forbidden");
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
