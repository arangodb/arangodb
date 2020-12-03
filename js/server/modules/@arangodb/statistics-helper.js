'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Statistics Helper
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2020 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Manuel Pöter
// / @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');

function MergeStatisticSamples(samples) {
  const merged = {
    physicalMemory: 0,
    residentSizeCurrent: 0,
    clientConnectionsCurrent: 0
  };

  const clusterIds = _.uniq(_.map(samples, s => s.clusterId));
  _.each(clusterIds, clusterId => {
    const sample = _.maxBy(_.filter(samples, s => s.clusterId === clusterId), s => s.time);
    merged.physicalMemory += sample.physicalMemory;
    merged.residentSizeCurrent += sample.residentSizeCurrent;
    merged.clientConnectionsCurrent += sample.clientConnectionsCurrent;
  });

  const paths = [
    "avgRequestTime",
    "bytesSentPerSecond",
    "bytesReceivedPerSecond",
    "http.optionsPerSecond",
    "http.putsPerSecond",
    "http.headsPerSecond",
    "http.postsPerSecond",
    "http.getsPerSecond",
    "http.deletesPerSecond",
    "http.othersPerSecond",
    "http.patchesPerSecond"
  ];

  const sampleTimes = _.map(samples, (s) => s.time);
  const empty = samples.length === 0;
  const minTime = empty ? 0 : Math.floor(_.min(sampleTimes));
  const maxTime = empty ? 0 : Math.ceil(_.max(sampleTimes));
  // the statisics samples are stored every 10 seconds, so our bucketSize should be a multiple of 10
  const bucketSize = 10;
  const numBuckets = Math.ceil((maxTime - minTime) / bucketSize);
  
  const calcBucket = (time) => Math.floor((time - minTime) / bucketSize);
  merged.times = _.range(minTime, minTime + numBuckets * bucketSize, bucketSize);

  _.each(paths, (path) => {
    const values = Array(numBuckets).fill(0);
    _.set(merged, path, values);
  });

  const counts = Array(numBuckets).fill(0);
  _.each(samples, (sample) => {
    const bucket = calcBucket(sample.time);
    ++counts[bucket];
    _.each(paths, (path) => {
      const values = _.get(merged, path);
      values[bucket] += _.get(sample, path);
    });
  });
  
  for (let i = 0; i < counts.length; ++i) {
    if (counts[i] !== 0) {
      merged.avgRequestTime[i] /= counts[i];
    }
  }

  return merged;
}

exports.MergeStatisticSamples = MergeStatisticSamples;
