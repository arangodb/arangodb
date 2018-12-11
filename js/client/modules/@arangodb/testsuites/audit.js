/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'audit': 'audit log tests'
};

const optionsDocumentation = [
  '   - `skipAudit` : if set to true the audit log tests are skipped'
];

const _ = require('lodash');
const fs = require('fs');
const tu = require('@arangodb/test-utils');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  audit: [tu.pathForTesting('server/audit')]
};

const sharedConf = {
  'audit.output': 'file://' + fs.getTempFile()
};

function auditLog(options) {
  if (options.skipAudit === true) {
    print('skipping audit log tests!');
    return {
      auditLog: {
        status: true,
        skipped: true
      }
    };
  }
  
  let opts = {
    audit: {
      name: 'audit',
      conf: sharedConf
    }
  };

  print(CYAN + 'Audit log tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.audit);

  return tu.performTests(options, testCases, 'audit', tu.runThere, opts.audit.conf);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['audit'] = auditLog;

  // turn off test by default.
  opts['skipAudit'] = true;

  // only enable them in enterprise version
  let version = {};
  if (global.ARANGODB_CLIENT_VERSION) {
    version = global.ARANGODB_CLIENT_VERSION(true);
    if (version['enterprise-version']) {
      opts['skipAudit'] = false;
    }
  }

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
