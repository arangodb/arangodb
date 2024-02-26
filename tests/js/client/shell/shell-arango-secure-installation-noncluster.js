/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let fs = require('fs');
let pu = require('@arangodb/testutils/process-utils');

function arangoSecureInstallationSuite () {
  'use strict';

  let oldPasswd;

  const arangoSecureInstallation = pu.ARANGO_SECURE_INSTALLATION_BIN;

  assertTrue(fs.isFile(arangoSecureInstallation), "arango-secure-installation not found!");

  return {
    setUpAll: function () {
      oldPasswd = internal.env['ARANGODB_DEFAULT_ROOT_PASSWORD'];
    },
    
    tearDownAll: function () {
      internal.env['ARANGODB_DEFAULT_ROOT_PASSWORD'] = oldPasswd;
    },

    setUp: function () {
      delete internal.env['ARANGODB_DEFAULT_ROOT_PASSWORD'];
    },
    
    testInvokeArangoSecureInstallationWithoutPassword: function () {
      let platform = internal.platform;
      if (platform !== 'linux') {
        // test is currently only supported on Linux
        return;
      }

      let path = fs.getTempFile();
      // database directory
      fs.makeDirectory(path);

      // set no password for the database
      try {
        let args = [path];
        // invoke arango-secure-installation without password. this will fail
        let actualRc = internal.executeExternalAndWait(arangoSecureInstallation, args);
        assertTrue(actualRc.hasOwnProperty("exit"));
        assertEqual(1, actualRc.exit);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },

    testInvokeArangoSecureInstallationWithPassword: function () {
      let platform = internal.platform;
      if (platform !== 'linux') {
        // test is currently only supported on Linux
        return;
      }

      let path = fs.getTempFile();
      // database directory
      fs.makeDirectory(path);

      // set an initial password for the database
      internal.env['ARANGODB_DEFAULT_ROOT_PASSWORD'] = 'haxxmann';
      try {
        let args = [path];
        // invoke arango-secure-installation with password. this must succeed
        let actualRc = internal.executeExternalAndWait(arangoSecureInstallation, args);
        assertTrue(actualRc.hasOwnProperty("exit"));
        assertEqual(0, actualRc.exit);
      } finally {
        try {
          fs.removeDirectory(path);
        } catch (err) {}
      }
    },
    
  };
}

jsunity.run(arangoSecureInstallationSuite);

return jsunity.done();
