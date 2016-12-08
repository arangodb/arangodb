/* global describe, beforeEach, afterEach, it, instanceInfo */
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
const internal = require('internal');
const download = require('internal').download;

let coordinator = instanceInfo.arangods.filter(arangod => {
  return arangod.role === 'coordinator';
})[0];

let dbServerCount = instanceInfo.arangods.filter(arangod => {
  return arangod.role === 'dbserver';
}).length;

describe('Shard distribution', function () {
  afterEach(function() {
    internal.db._drop('distributionTest');
  });
  it('should properly distribute a collection', function() {
    internal.db._create('distributionTest', {replicationFactor: 2, numberOfShards: 16});
    let distribution = JSON.parse(download(coordinator.url + '/_admin/cluster/shardDistribution').body).results;

    let leaders = Object.keys(distribution.distributionTest.Current).reduce((current, shardKey) => {
      let shard = distribution.distributionTest.Current[shardKey];
      if (current.indexOf(shard.leader) === -1) {
        current.push(shard.leader);
      }
      return current;
    }, []);
    expect(leaders).to.have.length.of.at.least(2);
  });
  it('should properly distribute a collection with full replication factor', function() {
    internal.db._create('distributionTest', {replicationFactor: dbServerCount, numberOfShards: 16});
    let distribution = JSON.parse(download(coordinator.url + '/_admin/cluster/shardDistribution').body).results;

    let leaders = Object.keys(distribution.distributionTest.Current).reduce((current, shardKey) => {
      let shard = distribution.distributionTest.Current[shardKey];
      if (current.indexOf(shard.leader) === -1) {
        current.push(shard.leader);
      }
      return current;
    }, []);
    expect(leaders).to.have.length.of.at.least(2);
  });
});
