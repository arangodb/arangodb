/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertMatch, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Copyright 2019, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require('internal');


const isEnterprise = internal.isEnterprise();
const isCluster = internal.isCluster();

function OneShardPropertiesSuite () {
  var dn = "UnitTestsDB";

  return {
    setUp: function () {
      try {
        db._useDatabase("_system");
        db._dropDatabase(dn);
      } catch(ex) {

      }
    },

    tearDown: function () {
      try {
        db._useDatabase("_system");
        db._dropDatabase(dn);
      } catch(ex) {
      }
    },

    testDefaultValues : function () {
      assertTrue(db._createDatabase(dn));
      db._useDatabase(dn);
      let props = db._properties();
      assertEqual(props.sharding, "");
      assertEqual(props.replicationFactor, 1);
    },

    testSingleSatellite : function () {
      assertTrue(db._createDatabase(dn, { sharding : "single", replicationFactor : "satellite"}));
      db._useDatabase(dn);
      let props = db._properties();
      assertEqual(props.sharding, "single");
      if (isEnterprise) {
        assertEqual(props.replicationFactor, "satellite");
      } else {
        assertEqual(props.replicationFactor, 1);
      }

      {
        let col = db._create("oneshardcol");
        let colProperties = col.properties();
        let graphsProperties = db._collection("_graphs").properties();

        if(isCluster && isEnterprise) {
          assertEqual(colProperties.distributeShardsLike, "_graphs");
          assertEqual(colProperties.replicationFactor, graphsProperties.replicationFactor);
        }
      }


      {
        let col = db._create("normalcol", { distributeShardsLike: ""});
        let colProperties = col.properties();
        let graphsProperties = db._collection("_graphs").properties();

        if(isCluster) {
          assertEqual(colProperties.distributeShardsLike, undefined);
          assertEqual(colProperties.replicationFactor, db._properties().replicationFactor);
        }
      }


      {
        let col = db._create("normalcolrepl5", { distributeShardsLike: "", replicationFactor:2});
        let colProperties = col.properties();
        let graphsProperties = db._collection("_graphs").properties();

        if(isCluster) {
          print(db._properties())
          assertEqual(colProperties.distributeShardsLike, undefined);
          assertEqual(colProperties.replicationFactor, 2);
        }
      }

    },
  };
}

jsunity.run(OneShardPropertiesSuite);
return jsunity.done();
