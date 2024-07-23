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
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const Minimatch = require('minimatch').Minimatch;
const mocha = require('@arangodb/mocha');

const isNotPattern = (pattern) => pattern.indexOf('*') === -1;

exports.run = function runFoxxTests (service, {filter, reporter}) {
  const run = (file, context) => service.run(file, {context});
  const result = mocha.run(run, exports.findTests(service), reporter, filter);
  if (reporter === 'xunit' && Array.isArray(result) && result[1]) {
    result[1].name = service.mount;
  }
  return result;
};

exports.findTests = function findTestFiles (service) {
  const patterns = service.manifest.tests || [];
  if (patterns.every(isNotPattern)) {
    return patterns.slice();
  }
  const basePath = fs.join(service.root, service.path);
  const paths = fs.listTree(basePath);
  const matchers = patterns.map((pattern) => {
    if (pattern.charAt(0) === '/') {
      pattern = pattern.slice(1);
    } else if (pattern.charAt(0) === '.' && pattern.charAt(1) === '/') {
      pattern = pattern.slice(2);
    }
    return new Minimatch(pattern);
  });
  return paths.filter(
    (path) => path && matchers.some((pattern) => pattern.match(path)) && fs.isFile(fs.join(basePath, path))
  );
};
