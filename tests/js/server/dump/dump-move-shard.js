/* jshint strict: false, sub: true */
/* global print, arango, assertEqual, assertTrue, assertFalse */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// DISCLAIMER
//
// Copyright 2016-2019 ArangoDB GmbH, Cologne, Germany
// Copyright 2014 triagens GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is ArangoDB GmbH, Cologne, Germany
//
// @author Kaveh Vahedipour
// //////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function dumpTestSuite () {
  'use strict';
  var db = internal.db;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test the empty collection
////////////////////////////////////////////////////////////////////////////////

    testMoveShard : function () {

      if (arango.getRole() !== "COORDINATOR") {
        return;
      }

      let database = "UnitTestsDumpDst";
      let collection = "UnitTestsDumpReplicationFactor1";
      let planShards =
          arango.GET("_admin/cluster/shardDistribution").results[collection].Plan;

      /*eslint no-extend-native: ["error", { "exceptions": ["Array"] }]*/
      Array.prototype.remove = function() {
        var what, a = arguments, L = a.length, ax;
          while (L && this.length) {
          what = a[--L];
          while ((ax = this.indexOf(what)) !== -1) {
            this.splice(ax, 1);
          }
        }
        return this;
      };

      var i = 0;
      var pending = [];
      Object.keys(planShards).forEach(
        function (shard) {
          let dbs = ["DBServer0001", "DBServer0002", "DBServer0003"];
          let leader = planShards[shard].leader;
          let follower = planShards[shard].followers[0];
          dbs = dbs.filter((d) => d !== leader && d !== follower);
          let unused = dbs[0];
          var toServer, fromServer;
          let modulo = i % 4;

          switch (modulo) {
          case 0:
            fromServer = leader; toServer = unused; break;
          case 1:
            fromServer = follower; toServer = unused; break;
          case 2:
            fromServer = leader; toServer = follower; break;
          default:
            return; break;
          }

          let body = {fromServer, toServer, database, collection, shard};
          let result = arango.POST("_admin/cluster/moveShard", body);
          assertTrue(!result.error);
          assertTrue(result.code === 202);
          pending.push(result.id);
          i++;
        });

      while (pending.length > 0) { // wait for moveShard jobs to finish
        var done = [];
        pending.forEach(
          function (jobId) {
            let query = arango.GET("/_admin/cluster/queryAgencyJob?id=" + jobId);
            if (query.status === "Finished") {
              done.push(jobId);
            }
          }
        );
        done.forEach( function(jobId) { pending.remove(jobId); });
        require("internal").sleep(0.25);
      }
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(dumpTestSuite);

return jsunity.done();
