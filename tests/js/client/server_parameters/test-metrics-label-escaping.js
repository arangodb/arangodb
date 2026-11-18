/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names': "true",
  };
}

const jsunity = require('jsunity');
const internal = require('internal');
const db = require('@arangodb').db;
const { instanceRole } = require('@arangodb/testutils/instance');
const IM = global.instanceManager;

const cn = "UnitTestsMetricsLabels";
const metricName = "arangodb_search_num_docs";

// legal index names under extended names and how they must appear in a label
const indexes = [
  { name: 'inv",injected="1\\x', label: 'index="inv\\",injected=\\"1\\\\x"' },
  { name: 'broken"', label: 'index="broken\\""' },
];

const metricLines = () => {
  const role = internal.isCluster() ? instanceRole.dbserver : instanceRole.single;
  let lines = [];
  IM.getInstancesRole(role).forEach((arangod) => {
    lines = lines.concat(arangod.getAllMetric().split('\n')
      .filter((line) => line.startsWith(metricName)));
  });
  return lines;
};

function testSuite() {
  return {
    setUp: function () {
      let c = db._create(cn);
      indexes.forEach((index, i) => {
        c.ensureIndex({ type: 'inverted', name: index.name, fields: ['f' + i] });
      });
      c.insert([{ f0: 1, f1: 1 }]);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testIndexNamesAreEscapedInLabels: function () {
      const lines = metricLines();
      indexes.forEach((index) => {
        assertTrue(lines.some((line) => line.includes(index.label)), lines);
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
