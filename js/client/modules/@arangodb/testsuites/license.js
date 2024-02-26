/* jshint strict: false, sub: true */
/* global print */
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
// / @author Kaveh Vahedipour
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'license': 'license tests'
};
const optionsDocumentation = [
  '   - `skipLicense` : if set to true the license tests are skipped',
];

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');
const fs = require('fs');
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'license': [tu.pathForTesting('client/license')],
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Shared conf
// //////////////////////////////////////////////////////////////////////////////

exports.setup = function(testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  // just a convenience wrapper for the regular tests
  testFns['license'] = ['license-api'];

  // only enable them in Enterprise Edition
  opts['skipLicense'] = !isEnterprise();

  for (var attrname in functionsDocumentation) {
    fnDocs[attrname] = functionsDocumentation[attrname];
  }
  for (var i = 0; i < optionsDocumentation.length; i++) {
    optionsDoc.push(optionsDocumentation[i]);
  }
};
