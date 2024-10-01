/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual */

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
/// @author Heiko Kernbach
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const internal = require('internal');
const base64Encode = require('internal').base64Encode;
const fs = require('fs');
const path = require('path');
const request = require("@arangodb/request");
const utils = require('@arangodb/foxx/manager-utils');
const FoxxManager = require('@arangodb/foxx/manager');

const getCoordinatorEndpoints = require('@arangodb/test-helper').getCoordinatorEndpoints;

let coordinators = []; // will be initialized during setup
const installMountPath = `/aRandomMountPathName`;
const databaseName = '_system';

const loadFoxxIntoZip = (path) => {
  let zip = utils.zipDirectory(path);
  let content = fs.readFileSync(zip);
  fs.remove(zip);
  return {
    type: 'inlinezip',
    buffer: content,
    filename: 'myFoxxApplication.zip',
    name: 'file'
  };
};

const getEncodedFoxxZipFile = () => {
  const itzpapalotlPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'itzpapalotl');
  const itzpapalotlZip = loadFoxxIntoZip(itzpapalotlPath);
  return base64Encode(itzpapalotlZip);
};

function sendRequest(method, endpoint, body, usePrimary) {
  let res;
  const i = usePrimary ? 0 : 1;

  try {
    const envelope = {
      body,
      method,
      url: `${coordinators[i]}${endpoint}`
    };
    res = request(envelope);
  } catch (err) {
    console.error(`Exception processing ${method} ${endpoint}`, err.stack);
    return {};
  }
  assertTrue(res.hasOwnProperty('body'), JSON.stringify(res));
  let resultBody = res.body;
  if (typeof resultBody === "string") {
    resultBody = JSON.parse(resultBody);
  }
  return resultBody;
}

const uploadFoxxZipFile = (databaseName, usePrimary, encodedZip) => {
  const uploadPath = `/_db/${databaseName}/_api/upload?multipart=true`;
  const uploadBody = JSON.stringify(encodedZip);
  const uploadResult = sendRequest('POST', uploadPath, uploadBody, usePrimary);
  assertTrue(uploadResult.filename);
  assertTrue(typeof uploadResult.filename === 'string');
  assertTrue(uploadResult.coordinatorId);
  assertTrue(typeof uploadResult.coordinatorId === 'string');

  return {
    fileName: uploadResult.filename,
    coordinatorId: uploadResult.coordinatorId
  };
};

const installFoxxZipFile = (databaseName, usePrimary, fileName, coordinatorId, expectedFailureCode = undefined) => {
  const installBody = {
    zipFile: `${fileName}`,
  };
  if (coordinatorId) {
    installBody.coordinatorId = coordinatorId;
  }

  const installPath = `/_db/${databaseName}/_admin/aardvark/foxxes/zip?mount=${encodeURIComponent(installMountPath)}&setup=true`;
  const installResult = sendRequest('PUT', installPath, JSON.stringify(installBody), usePrimary);
  assertTrue(installResult.hasOwnProperty('error'), JSON.stringify(installResult));
  if (expectedFailureCode) {
    // TODO: We want to be able to test this in more detail. But currently, foxx does not deliver proper ArangoErrors
    // in some failure cases. Created BTS-Ticket: https://arangodb.atlassian.net/browse/BTS-1345 needs to be resolved
    // first.
    assertTrue(installResult.error);
    assertEqual(installResult.errorNum, expectedFailureCode);
    assertEqual(installResult.code, expectedFailureCode);
  } else {
    assertFalse(installResult.error);
    assertEqual(installResult.mount, installMountPath);
  }

  return installResult;
};

const tryRemoveFoxx = (installMountPath) => {
  try {
    FoxxManager.uninstall(installMountPath);
  } catch (ignore) {
  }
};

function FoxxInstallationSuite() {
  'use strict';

  return {

    setUpAll: function () {
      coordinators = getCoordinatorEndpoints();
      if (coordinators.length < 2) {
        throw new Error('Expecting at least two coordinators');
      }
    },

    tearDown: function () {
      tryRemoveFoxx(installMountPath);
    },

    testFoxxZipUpload: function () {
      // uploads and install the service to the same coordinator
      const encodedZip = getEncodedFoxxZipFile();

      // upload the foxx application to the primary coordinator
      let usePrimary = true;
      const result = uploadFoxxZipFile(databaseName, usePrimary, encodedZip);
      let fileName = result.fileName;

      // send the installation request to another coordinator (secondary)
      installFoxxZipFile(databaseName, usePrimary, fileName, null, false);
    },

    testFoxxZipUploadWithSameCoordinatorID: function () {
      // uploads and install the service to the same coordinator using additional coordinatorId parameter
      const encodedZip = getEncodedFoxxZipFile();

      // upload the foxx application to the primary coordinator
      let usePrimary = true;

      const result = uploadFoxxZipFile(databaseName, usePrimary, encodedZip);
      const fileName = result.fileName;
      const coordinatorId = result.coordinatorId;

      // send the installation request to another coordinator (secondary)
      installFoxxZipFile(databaseName, usePrimary, fileName, coordinatorId, false);
    },

    testFoxxZipUploadWithRedirect: function () {
      // uploads the service to the first coordinator and requests the installation on the second coordinator
      // by using a coordinatorId (of the first coordinator)
      const encodedZip = getEncodedFoxxZipFile();

      // upload the foxx application to the primary coordinator
      let usePrimary = true;

      const result = uploadFoxxZipFile(databaseName, usePrimary, encodedZip);
      const fileName = result.fileName;
      const coordinatorId = result.coordinatorId;

      // flip coordinator, now using the secondary coordinator
      usePrimary = false;

      // send the installation request to another coordinator (secondary)
      installFoxxZipFile(databaseName, usePrimary, fileName, coordinatorId, false);
    },

    testFoxxZipUploadWithMissingCoordIdRedirect: function () {
      // uploads the service to the first coordinator and requests the installation on the second coordinator
      // without specifying the proper coordinator id (expected to fail)
      const encodedZip = getEncodedFoxxZipFile();

      // upload the foxx application to the primary coordinator
      let usePrimary = true;


      const result = uploadFoxxZipFile(databaseName, usePrimary, encodedZip);
      const fileName = result.fileName;

      // flip coordinator, now using the secondary coordinator
      usePrimary = false;

      // send the installation request to another coordinator (secondary)
      installFoxxZipFile(databaseName, usePrimary, fileName, null, 400);
    },

    testFoxxZipUploadWithInvalidCoordIdRedirect: function () {
      // uploads the service to the first coordinator and requests the installation on the second coordinator
      // without specifying the proper coordinator id (expected to fail)
      const encodedZip = getEncodedFoxxZipFile();

      // upload the foxx application to the primary coordinator
      let usePrimary = true;


      const coordinatorId = "I-AM-AN-INVALID-COORDINATOR-ID";
      const result = uploadFoxxZipFile(databaseName, usePrimary, encodedZip);
      const fileName = result.fileName;

      // flip coordinator, now using the secondary coordinator
      usePrimary = false;

      // send the installation request to another coordinator (secondary)
      installFoxxZipFile(databaseName, usePrimary, fileName, coordinatorId, 400);
    }

  };
}

jsunity.run(FoxxInstallationSuite);

return jsunity.done();
