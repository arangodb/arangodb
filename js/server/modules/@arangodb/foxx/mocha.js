'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

var fs = require('fs');
var Minimatch = require('minimatch').Minimatch;
var isWindows = require('internal').platform.substr(0, 3) === 'win';

const mocha = require('@arangodb/mocha');

exports.run = function runFoxxTests(app, reporterName) {
  function run(file, context) {
    return app.run(file, {context: context});
  }
  return mocha.run(run, findTestFiles(app), reporterName);
};

function isNotPattern(pattern) {
  return pattern.indexOf('*') === -1;
}

function findTestFiles(app) {
  var patterns = app.manifest.tests || [];
  if (patterns.every(isNotPattern)) {
    return patterns.slice();
  }
  var basePath = fs.join(app.root, app.path);
  var paths = fs.listTree(basePath);
  var matchers = patterns.map(function (pattern) {
    if (pattern.charAt(0) === '/') {
      pattern = pattern.slice(1);
    } else if (pattern.charAt(0) === '.' && pattern.charAt(1) === '/') {
      pattern = pattern.slice(2);
    }
    return new Minimatch(pattern);
  });
  return paths.filter(function (path) {
    return path && matchers.some(function (pattern) {
      return pattern.match(
        isWindows ? path.replace(/\\/g, '/') : path
      );
    }) && fs.isFile(fs.join(basePath, path));
  });
}
