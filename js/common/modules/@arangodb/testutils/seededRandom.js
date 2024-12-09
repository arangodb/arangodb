/*jshint strict: true */
/*global print*/
'use strict';
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

// This is a seedable RandomNumberGenerator
// it is not perfect for Random numbers,
// but good enough for what we are doing here
function randomNumberGeneratorInt(seed) {
  const rng = (function* (seed) {
    while (true) {
      const nextVal = Math.cos(seed++) * 10000;
      yield nextVal - Math.floor(nextVal);
    }
  })(seed);

  return function () {
    return rng.next().value;
  };
}

function randomNumberGeneratorFloat(seed) {
  const rng = (function* (seed) {
    while (true) {
      const nextVal = Math.cos(seed++);
      yield nextVal;
    }
  })(seed);

  return function () {
    return rng.next().value;
  };
}

exports.randomNumberGeneratorInt = randomNumberGeneratorInt;
exports.randomNumberGeneratorFloat = randomNumberGeneratorFloat;
