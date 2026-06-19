/*jshint globalstrict:false, strict:false */
/*global */

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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");
let IM = global.instanceManager;

function AuthSuite() {
  'use strict';

  const user1 = 'hackers@arango.ai';
  const user2 = 'noone@arango.ai';

  const cn = 'UnitTestsCollection';
      
  return {
    setUpAll: function () {
      IM.rememberConnection();

      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }
    
      try {
        db._dropDatabase('UnitTestsDatabase');
      } catch (err) {
      }
      
      db._createDatabase('UnitTestsDatabase');
      
      users.save(user1, "foobar");
      users.save(user2, "foobar");
      users.grantDatabase(user1, 'UnitTestsDatabase');
      users.grantCollection(user1, 'UnitTestsDatabase', "*");
      users.grantDatabase(user2, 'UnitTestsDatabase', 'ro');
      users.reload();
    },

    tearDownAll: function () {
      IM.reconnectMe();
      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }

      try {
        db._dropDatabase('UnitTestsDatabase');
      } catch (err) {
      }
    },
    
    tearDown: function () {
      arango.reconnect(IM.endpoint, 'UnitTestsDatabase', "root", "");
      db._drop(cn);

      IM.reconnectMe();
      db._drop(cn);
    },
    
    testRestoreSystemDBRoot: function () {
      IM.reconnectMe();
    
      let result = arango.PUT('/_api/replication/restore-collection', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
    },
    
    testRestoreSystemDBRootOverwrite: function () {
      IM.reconnectMe();
    
      let result = arango.PUT('/_api/replication/restore-collection', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
      
      result = arango.PUT('/_api/replication/restore-collection?overwrite=true', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
      
      result = arango.PUT('/_api/replication/restore-collection?overwrite=false', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.error);
      assertEqual(409, result.code);
      
      result = arango.PUT('/_api/replication/restore-collection?overwrite=true', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
    },
    
    testRestoreOtherDBRoot: function () {
      arango.reconnect(IM.endpoint, 'UnitTestsDatabase', 'root', '');
    
      let result = arango.PUT('/_api/replication/restore-collection', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
    },
    
    testRestoreOtherDBRW: function () {
      arango.reconnect(IM.endpoint, 'UnitTestsDatabase', user1, 'foobar');
    
      let result = arango.PUT('/_api/replication/restore-collection', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
    },
    
    testRestoreOtherDBRWOverwrite: function () {
      arango.reconnect(IM.endpoint, 'UnitTestsDatabase', user1, 'foobar');
    
      let result = arango.PUT('/_api/replication/restore-collection', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
      
      result = arango.PUT('/_api/replication/restore-collection?overwrite=true', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
      
      result = arango.PUT('/_api/replication/restore-collection?overwrite=false', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.error);
      assertEqual(409, result.code);
      
      result = arango.PUT('/_api/replication/restore-collection?overwrite=true', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.result);
    },
    
    testRestoreOtherDBRO: function () {
      arango.reconnect(IM.endpoint, 'UnitTestsDatabase', user2, 'foobar');
    
      let result = arango.PUT('/_api/replication/restore-collection', {
        parameters: { name: cn, type: 2 }, indexes: [] 
      });
      assertTrue(result.error);
      assertEqual(403, result.code);
    },
  };
}


jsunity.run(AuthSuite);

return jsunity.done();
