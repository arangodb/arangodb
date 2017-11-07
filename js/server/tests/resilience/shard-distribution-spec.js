/* global describe, beforeEach, afterEach, it, instanceInfo, before */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
'use strict';

const expect = require('chai').expect;
const assert = require('chai').assert;
const internal = require('internal');
const download = require('internal').download;
const colName = "UnitTestDistributionTest";
const _ = require("lodash");
const request = require('@arangodb/request');

let coordinator = instanceInfo.arangods.filter(arangod => {
  return arangod.role === 'coordinator';
})[0];

let dbServerCount = instanceInfo.arangods.filter(arangod => {
  return arangod.role === 'dbserver';
}).length;

describe('Shard distribution', function () {

  before(function() {
    internal.db._drop(colName);
  });

  afterEach(function() {
    internal.db._drop(colName);
  });

  it('should properly distribute a collection', function() {
    internal.db._create(colName, {replicationFactor: 2, numberOfShards: 16});
    var d = request.get(coordinator.url + '/_admin/cluster/shardDistribution');
    let distribution = JSON.parse(d.body).results;
    
    let leaders = Object.keys(distribution[colName].Current).reduce((current, shardKey) => {
      let shard = distribution[colName].Current[shardKey];
      if (current.indexOf(shard.leader) === -1) {
        current.push(shard.leader);
      }
      return current;
    }, []);
    expect(leaders).to.have.length.of.at.least(2);
  });

  describe('the format with full replication factor', function () {

    let distribution;
    const nrShards = 16;

    before(function () {
      internal.db._create(colName, {replicationFactor: dbServerCount, numberOfShards: nrShards});
      var d = request.get(coordinator.url + '/_admin/cluster/shardDistribution');
      distribution = JSON.parse(d.body).results[colName];
      assert.isObject(distribution, 'The distribution for each collection has to be an object');
    });

    it('should have current and plan on top level', function () {
      expect(distribution).to.have.all.keys(["Current", "Plan"]);
      assert.isObject(distribution.Current, 'The Current has to be an object');
      assert.isObject(distribution.Plan, 'The Current has to be an object');
    });

    it('should list identical shards in Current and Plan', function () {
      let keys = Object.keys(distribution.Plan);
      expect(keys.length).to.equal(nrShards);
      // Check that keys (shardnames) are identical 
      expect(distribution.Current).to.have.all.keys(distribution.Plan);
    });

    it('should have the correct format of each shard in Plan', function () {
      _.forEach(distribution.Plan, function (info, shard) {
        if (info.hasOwnProperty('progress')) {
          expect(info).to.have.all.keys(['leader', 'progress', 'followers']);
          expect(info.progress).to.have.all.keys(['total', 'current']);
        } else {
          expect(info).to.have.all.keys(['leader', 'followers']);
        }
        expect(info.leader).to.match(/^DBServer/);
        assert.isArray(info.followers, 'The followers need to be an array');
        // We have one replica for each server, except the leader
        expect(info.followers.length).to.equal(dbServerCount - 1);
        _.forEach(info.followers, function (follower) {
          expect(follower).to.match(/^DBServer/);
        });
      });
    });

    it('should have the correct format of each shard in Current', function () {
      _.forEach(distribution.Current, function (info, shard) {
        expect(info).to.have.all.keys(['leader', 'followers']);

        expect(info.leader).to.match(/^DBServer/);
        assert.isArray(info.followers, 'The followers need to be an array');

        // We have at most one replica per db server. They may not be in sync yet.
        expect(info.followers.length).to.below(dbServerCount);
        _.forEach(info.followers, function (follower) {
          expect(follower).to.match(/^DBServer/);
        });
      });
    });

    it('should distribute shards on all servers', function () {
      let leaders = new Set();
      _.forEach(distribution.Plan, function (info, shard) {
        leaders.add(info.leader);
      });
      expect(leaders.size).to.equal(Math.min(dbServerCount, nrShards));
    });

  });

});
