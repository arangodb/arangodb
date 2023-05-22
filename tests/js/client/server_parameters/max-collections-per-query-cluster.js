/*jshint globalstrict:false, strict:false */
/*global assertEqual, fail, getOptions*/

////////////////////////////////////////////////////////////////////////////////
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

if (getOptions === true) {
  return {
    'query.max-collections-per-query': '400',
  };
}

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const errors = arangodb.errors;
const { getEndpointsByType, debugCanUseFailAt, debugSetFailAt, debugClearFailAt } = require('@arangodb/test-helper');
const ep = getEndpointsByType('coordinator');

function OptionsTestSuite () {
  const cn = "UnitTestsCollection";

  return {

    setUp : function () {
      // make all shards end up on the same DB server
      ep.forEach((ep) => {
        debugSetFailAt(ep, "allShardsOnSameServer");
      });

      for (let i = 0; i < 5; ++i) {
        let options = { numberOfShards: 100, replicationFactor: 1 };
        if (i > 0) {
          options.distributeShardsLike = cn + "0";
        }
        db._create(cn + i, options);
      }
    },

    tearDown : function () {
      ep.forEach((ep) => debugClearFailAt(ep));
      // must delete in reverse order because of distributeShardsLike
      for (let i = 5; i > 0; --i) {
        db._drop(cn + (i - 1));
      }
    },

    testTooManyCollections : function () {
      for (let i = 0; i < 5; ++i) {
        let parts = [];
        for (let j = 0; j <= i; ++j) {
          parts.push("FOR d" + j + " IN " + cn + j);
        }
        let query = parts.join(" ") + " RETURN 1";
        if (i <= 3) {
          let res = db._query(query).toArray();
          assertEqual(0, res.length);
        } else {
          try {
            db._query(query);
            fail();
          } catch (err) {
            assertEqual(errors.ERROR_QUERY_TOO_MANY_COLLECTIONS.code, err.errorNum);
          }
        }
      }
    },

  };
}

if (ep.length && debugCanUseFailAt(ep[0])) {
  jsunity.run(OptionsTestSuite);
}
return jsunity.done();
