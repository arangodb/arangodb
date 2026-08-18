/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, arango, getOptions */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// /
// / The Programs (which include both the software and documentation) contain
// / proprietary information of ArangoDB GmbH; they are provided under a license
// / agreement containing restrictions on use and disclosure and are also
// / protected by copyright, patent and other intellectual and industrial
// / property laws. Reverse engineering, disassembly or decompilation of the
// / Programs, except to the extent required to obtain interoperability with
// / other independently created software or as specified by law, is prohibited.
// /
// / It shall be the licensee's responsibility to take all appropriate fail-safe,
// / backup, redundancy, and other measures to ensure the safe use of
// / applications if the Programs are used for purposes such as nuclear,
// / aviation, mass transit, medical, or other inherently dangerous applications,
// / and ArangoDB GmbH disclaims liability for any damages caused by such use of
// / the Programs.
// /
// / This software is the confidential and proprietary information of ArangoDB
// / GmbH. You shall not disclose such confidential and proprietary information
// / and shall use it only in accordance with the terms of the license agreement
// / you entered into with ArangoDB GmbH.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////


// The location of this file is currently needed such that we create a new
// arangod instance on startup with the specific failure point (routes are
// registered with their allowed api versions already on startup).
// Once the failure point goes away, we can move the test to
// tests/js/client/authentication.

// API version 1 is not yet in ApiVersion.h's supportedApiVersions, so it is
// normally rejected before any handler runs (see GeneralRequest.cpp and
// RestHandlerFactory.cpp). The 'ApiVersion::treatVersion1AsSupported' failure
// point makes those two checks treat version 1 as supported, without
// touching supportedApiVersions itself. It must be armed before the server
// registers its REST routes at startup, so it is passed as a startup option
// here rather than set later via the usual debugSetFailAt() runtime call.
if (getOptions === true) {
  return {
    'server.failure-point': 'ApiVersion::treatVersion1AsSupported',
    'server.authentication': 'true'
  };
}

const jsunity = require("jsunity");
const {db} = require('@arangodb');
const users = require("@arangodb/users");

const IM = require('@arangodb/test-helper').getInstanceInfo();

const DB_NAME = "my-db";
const COLLECTION_NAME = "my-collection";
const USER = "user";
const PASSWORD = "password";

function failingUserAccessDoesNotRevealInformationSuite () {
   // Makes all subsequent requests of the arangosh connection run as USER
   // Connecting requires read access to the database that is connected to.
  const connectAsUser = () => {
    arango.reconnect(arango.getEndpoint(), '_system', USER, PASSWORD);
  };

  return {
    setUpAll: function () {
      // remember the root connection, so that it can be restored later on
      IM.rememberConnection();
    },

    setUp: function() {
      IM.reconnectMe();
      db._useDatabase('_system');
      users.save(USER, PASSWORD);
      users.grantDatabase(USER, '_system', "ro");
      users.reload();
      db._createDatabase(DB_NAME);
      db._useDatabase(DB_NAME);
      db._createDocumentCollection(COLLECTION_NAME);
      
    },

    tearDown: function () {
      IM.reconnectMe();
      db._useDatabase('_system');
      db._dropDatabase(DB_NAME);
      users.remove(USER);
      db._drop(COLLECTION_NAME);
    },

    tearDownAll: function () {
      IM.reconnectMe();
    },

    testUseDatabase () {
      // with read permissions user can see db
      users.grantDatabase(USER, DB_NAME, "ro");
      users.reload();
      connectAsUser();
      assertFalse(arango.GET("/_arango/v1/_db/my-db/_api/collection").error);
      
      IM.reconnectMe();
      users.grantDatabase(USER, DB_NAME, "none");
      users.reload();
      connectAsUser();
      const non_existing_db_error = arango.GET("/_arango/v1/_db/does-not-exist/_api/collection");
      const no_access_to_db_error = arango.GET("/_arango/v1/_db/my-db/_api/collection");
      assertEqual(no_access_to_db_error, non_existing_db_error);
    },
    
    testUseCollection () {
      users.grantDatabase(USER, DB_NAME, "rw");

      // with read permissions user can see collection
      users.grantCollection(USER, DB_NAME, "my-collection", "ro");
      users.reload();
      connectAsUser();
      assertFalse(arango.GET("/_arango/v1/_db/my-db/_api/collection/my-collection").error);

      IM.reconnectMe();
      users.grantCollection(USER, DB_NAME, "my-collection", "none");
      users.reload();
      connectAsUser();
      const non_existing_collection_error = arango.GET("/_arango/v1/_db/my-db/_api/collection/non-existing-collection");
      const no_access_to_collection_error = arango.GET("/_arango/v1/_db/my-db/_api/collection/my-collection");
      assertEqual(no_access_to_collection_error, non_existing_collection_error);

    }, 
  };
}

jsunity.run(failingUserAccessDoesNotRevealInformationSuite);

return jsunity.done();
