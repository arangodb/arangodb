/* jshint strict: false, sub: true */
/* global print db arango */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks no tasks were left on the SUT by tests
// //////////////////////////////////////////////////////////////////////////////

const analyzers = require("@arangodb/analyzers");

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks that no new analyzers were left on the SUT. 
// //////////////////////////////////////////////////////////////////////////////
exports.checker = class {
  constructor(runner) {
    this.runner = runner;
    this.name = 'analyzers';
    this.analyzersBefore = [];
  }
  setUp(te) {
    this.analyzersBefore = [];
    analyzers.toArray().forEach(oneAnalyzer => {
      this.analyzersBefore.push(oneAnalyzer.name());
    });
    return true;
  }
  runCheck(te) {
    let leftover = [];
    let foundAnalyzers = [];
    try {
      analyzers.toArray().forEach(oneAnalyzer => {
        let name = oneAnalyzer.name();
        let found = this.analyzersBefore.find(oneAnalyzer => oneAnalyzer === name);
        if (found === name) {
          foundAnalyzers.push(name);
        } else {
          leftover.push(name);
        }
      });
    } catch (x) {
      this.runner.setResult(te, false, {
        status: false,
        message: 'failed to fetch the currently available analyzers: [ ' + x.message
      });
      return false;
    }
    if (leftover.length !== 0) {
      this.runner.setResult(te, false, {
        status: false,
        message: 'Cleanup missing - test left over analyzer: [ ' + leftover
      });
      return false;
    } else if (foundAnalyzers.length !== this.analyzersBefore.length) {
      this.runner.setResult(te, false, {
        status: false,
        message: 'Cleanup remove analyzers: [ ' + foundAnalyzers  + ' != ' + this.analyzersBefore
      });
      return false;
    }
    return true;
  }
};
