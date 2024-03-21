/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const db = require("internal").db;
const url = require('url');
const _ = require("lodash");
const { getDBServers } = require('@arangodb/test-helper');

const cn = "UnitTestsQueries";

function TransactionCommitAbortOverwhelmSuite () {
  'use strict';
    
  let testFunc = (cb) => {
    // create exclusive transaction
    let trx = db._createTransaction({
      collections: { write: "d" }
    });

    try {
      // write 5 documents
      trx.collection('d').insert([{}, {}, {}, {}, {}]);

      const [shard, servers] = Object.entries(db.c.shards(true))[0];

      // trigger move shard
      let body = {database: cn, collection: "c", shard, fromServer: servers[0], toServer: servers[1]};
      let result = arango.POST("/_admin/cluster/moveShard", body);

      assertFalse(result.error);
      assertEqual(202, result.code);

      while (true) {
        const job = arango.GET(`/_admin/cluster/queryAgencyJob?id=${result.id}`);
        if (job.error === false && job.status === "Pending") {
          break;
        }
        require('internal').wait(0.5);
      }

      // create many small write to e f and f
      const header = {"X-Arango-Async": "store"};
      for (let i = 0; i < 1000; i++) {
        arango.POST(`/_api/document/e`, {}, header);
        arango.POST(`/_api/document/f`, {}, header);
        arango.POST(`/_api/document/g`, {}, header);
      }

      // commit transaction
      // should have priority over the inserts
      result = cb(trx); 
      trx = null;
    } finally {
      if (trx !== null) {
        trx.abort();
      }
    }
  };
  
  return {
    setUpAll: function () {
      db._createDatabase(cn);
      db._useDatabase(cn);
  
      db._create("c", {numberOfShards: 5, replicationFactor: 2});
      db._create("d", {distributeShardsLike: "c"});
      db._create("e", {distributeShardsLike: "c"});
      db._create("f", {distributeShardsLike: "c"});
      db._create("g", {distributeShardsLike: "c"});
    },
    
    tearDownAll: function () {
      db._useDatabase('_system');
      db._dropDatabase(cn);
    },

    testCommitNotAffectedByOverwhelm : function () {
      testFunc((trx) => {
        let result = trx.commit();
        assertEqual("committed", result.status);
        return result;
      });
    },
    
    testAbortNotAffectedByOverwhelm : function () {
      testFunc((trx) => {
        let result = trx.abort();
        assertEqual("aborted", result.status);
        return result;
      });
    },
    
  };
}

jsunity.run(TransactionCommitAbortOverwhelmSuite);
return jsunity.done();
