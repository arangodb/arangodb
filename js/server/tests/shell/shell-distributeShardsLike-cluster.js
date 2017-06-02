/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2010-2017 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var errors = require("internal").errors;

var db = arangodb.db;

function DistributeShardsLikeSuite() {
  'use strict';
  var cn1 = "UnitTestsDistributeShardsLike1";
  var cn2 = "UnitTestsDistributeShardsLike2";
  var cn3 = "UnitTestsDistributeShardsLike3";

  return {
    setUp: function() {
      db._drop(cn2);
      db._drop(cn3);
      db._drop(cn1);
    },

    tearDown: function() {
      db._drop(cn2);
      db._drop(cn3);
      db._drop(cn1);
    },

    testPointToEmpty: function() {
      try {
        db._create(cn1, {numberOfShards: 2, distributeShardsLike: cn2});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE.code,
                    err.errorNum);
      }
    },

    testAvoidChain: function() {
      db._create(cn1, {numberOfShards: 2});
      db._create(cn2, {numberOfShards: 2, distributeShardsLike: cn1});
      try {
        db._create(cn3, {numberOfShards: 2, distributeShardsLike: cn2});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE.code,
                    err.errorNum);
      }
    },

    testPreventDrop: function() {
      db._create(cn1, {numberOfShards: 2});
      db._create(cn2, {numberOfShards: 2, distributeShardsLike: cn1});
      db._create(cn3, {numberOfShards: 2, distributeShardsLike: cn1});
      try {
        db._drop(cn1);
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE.code,
                    err.errorNum);
      }
    }
  };
}

jsunity.run(DistributeShardsLikeSuite);
return jsunity.done();

