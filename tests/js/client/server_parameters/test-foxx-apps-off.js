/*jshint globalstrict:false, strict:false */
/* global getOptions, assertFalse, assertEqual, arango */
'use strict';

if (getOptions === true) {
  return {
    'foxx.apps': 'false'
  };
}

const jsunity = require('jsunity');
const fs = require('fs');
const internal = require('internal');
const pu = require('@arangodb/process-utils');

function testSuite() {
  const arangodb = require('@arangodb');
  const download = internal.download;
  const endpoint = arangodb.arango.getEndpoint().replace('tcp://', 'http://');

  return {
    setUp: function() {
      let result = arango.POST_RAW('/_admin/execute', `
          const FoxxManager = require('@arangodb/foxx/manager');
          const mount = '/testmount';
          try {
            FoxxManager.uninstall(mount, {force: true});
          } catch (err) {}
 
          const fs = require('fs');
          const basePath = fs.makeAbsolute(fs.join(require('internal').pathForTesting('common'), 'test-data', 'apps'));
          const foxxApp = fs.join(basePath, 'server-security');
          FoxxManager.install(foxxApp, mount); `);
      assertFalse(result.error);
      assertEqual(200, result.code);
    },

    tearDown: function() {
      let result = arango.POST_RAW('/_admin/execute', `
          const FoxxManager = require('@arangodb/foxx/manager');
          const mount = '/testmount';
          try {
            FoxxManager.uninstall(mount, {force: true});
          } catch (err) {}`);
      assertFalse(result.error);
      assertEqual(200, result.code);
    },

    // routes are defined in:
    // tests/js/common/test-data/apps/server-security/index.js

    testPid : function() {
      const mount = '/testmount';
      const url = endpoint + mount + "/pid";
      const result = download(url);
     
      assertEqual(404, result.code);
    },

  };
}


jsunity.run(testSuite);
return jsunity.done();
