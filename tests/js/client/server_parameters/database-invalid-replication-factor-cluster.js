/*jshint globalstrict:false, strict:true */
/*global getOptions, assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2016 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2023, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'cluster.min-replication-factor': '2',
    'cluster.max-replication-factor': '3',
    'cluster.default-replication-factor': '2',
  };
}

const jsunity = require("jsunity");
const wait = require("internal").sleep;
const errors = require("internal").errors;
const helper = require("@arangodb/test-helper");
const db = require("@arangodb").db;

function InvalidReplicationFactorTestSuite () {
  'use strict';

  const name = "someTestDatabase";
  
  return {
    tearDown : function () {
      try {
        db._dropDatabase(name);
      } catch (err) {
      }
    },
    
    testCreateDatabaseWithInvalidReplicationFactor : function () {
      try {
        db._createDatabase(name, { replicationFactor: 1 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testStoreDatabaseInPlanWithInvalidReplicationFactor : function () {
      // inject a database with replicationFactor 1 into the Plan,
      // bypassing the database creation API, which would validate the
      // replication factor
      const data = {
        "writeConcern": 1,
        "sharding": "single",
        "replicationFactor": 1,
        "name": name,
        "isSystem": false,
        "id": "3457693543845" // just assume this ID is not used otherwise
      };
      const key = "Plan/Databases/" + name;
      helper.agency.set(key, data);
      helper.agency.increaseVersion("Plan/Version");

      let res = helper.agency.get(key).arango.Plan.Databases[name];
      assertEqual(res, data);
   
      const dbServers = helper.getEndpointsByType("dbserver").length;

      let keys, current;
      let tries = 0;

      while (++tries < 50) {
        let res = helper.agency.get("Current/Databases").arango.Current.Databases;
        if (!res.hasOwnProperty(name)) {
          wait(0.5);
          continue;
        } 

        current = res[name];
        keys = Object.keys(current);
        if (keys.length < dbServers) {
          // not all DB servers reported in Current
          wait(0.5);
          continue;
        }

        break;
      }

      // all DB servers must have picked up the database and successfully
      // report it to Current, despite the replicationFactor that isnt'r right.
      keys.forEach((k) => {
        assertFalse(current[k].error);
        assertEqual(0, current[k].errorNum);
        assertEqual("", current[k].errorMessage);
        assertEqual(name, current[k].name);
      });
    },

  };
}


jsunity.run(InvalidReplicationFactorTestSuite);

return jsunity.done();
