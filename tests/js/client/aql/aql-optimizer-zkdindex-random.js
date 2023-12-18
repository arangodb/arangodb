/* global fail */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const aql = arangodb.aql;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");

function optimizerRuleZkd2dIndexTestSuite(unique) {
  const colName = 'UnitTestZkdIndexCollection';
  let col;
  let docs = [];
  const prefixValues = ["foo", "bar", "baz"];

  let points = new Set();
  const valueSize = 20;

  const makePoint = function () {
    let x, y;
    do {
      x = valueSize * Math.random();
      y = valueSize * Math.random();
      if (!unique) {
        break;
      }
    } while (points.has([x, y]));
    points.add([x, y]);
    return [x, y];
  };

  const computeExpectedResult = function (xStart, xEnd, yStart, yEnd, name) {
    return docs.filter(function (doc) {
      return xStart <= doc.x && doc.x <= xEnd && yStart <= doc.y && doc.y <= yEnd && doc.name === name;
    }).map(x => x.k).sort();
  };

  const deriveName = function (name) {
    return name + (unique ? "_Unique" : "_NonUnique");
  };

  const rng = () => 1.1 * valueSize * Math.random() - 0.05 * valueSize;
  return {
    setUpAll: function () {
      col = db._create(colName);
      col.ensureIndex({
        type: 'zkd',
        name: 'zkdIndex',
        fields: ['x', 'y'],
        storedValues: ['k'],
        unique: unique,
        fieldValueTypes: 'double',
        prefixFields: ['name']
      });

      for (let k = 0; k < 100000; k++) {
        const [x, y] = makePoint();
        docs.push({k, x, y, name: _.sample(prefixValues)});
      }
      col.save(docs);
    },

    tearDownAll: function () {
      col.drop();
    },

    [deriveName("testRandomRect")]: function () {

      for (let k = 0; k < 300; k++) {

        const [xStart, xEnd] = [rng(), rng()].sort();
        const [yStart, yEnd] = [rng(), rng()].sort();
        const name = _.sample(prefixValues);

        const result = db._query(aql`
              FOR doc IN ${col}
                FILTER doc.x >= ${xStart} && doc.x <= ${xEnd}
                FILTER doc.y >= ${yStart} && doc.y <= ${yEnd}
                FILTER doc.name == ${name}
                RETURN doc.k
            `).toArray().sort();

        const expected = computeExpectedResult(xStart, xEnd, yStart, yEnd, name);

        assertEqual(result, expected, [xStart, xEnd, yStart, yEnd, name]);
      }
    },

    [deriveName("testHalfSideOpen")]: function () {
      for (let k = 0; k < 300; k++) {
        const [xStart, xEnd] = [rng(), rng()].sort();
        const yStart = rng();
        const name = _.sample(prefixValues);

        const result = db._query(aql`
              FOR doc IN ${col}
                FILTER doc.x >= ${xStart} && doc.x <= ${xEnd}
                FILTER doc.y >= ${yStart}
                FILTER doc.name == ${name}
                RETURN doc.k
            `).toArray().sort();

        const expected = computeExpectedResult(xStart, xEnd, yStart, 2000, name);

        assertEqual(result, expected, [xStart, xEnd, yStart, 2000, name]);
      }
    },

    [deriveName("testEqualSlice")]: function () {
      for (let k = 0; k < 300; k++) {
        const [xStart, xEnd] = [rng(), rng()].sort();
        const yStart = rng();
        const name = _.sample(prefixValues);

        const result = db._query(aql`
              FOR doc IN ${col}
                FILTER doc.x >= ${xStart} && doc.x <= ${xEnd}
                FILTER doc.y == ${yStart}
                FILTER doc.name == ${name}
                RETURN doc.k
            `).toArray().sort();

        const expected = computeExpectedResult(xStart, xEnd, yStart, yStart, name);

        assertEqual(result, expected, [xStart, xEnd, yStart, yStart, name]);
      }
    }
  };
}

function makeTestSuite(unique) {
  return function () {
    return optimizerRuleZkd2dIndexTestSuite(unique);
  };
}

jsunity.run(makeTestSuite(true));
jsunity.run(makeTestSuite(false));

return jsunity.done();
