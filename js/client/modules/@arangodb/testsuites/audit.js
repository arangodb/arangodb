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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'audit_server': 'audit log tests executed on server',
  'audit_client': 'audit log tests executed in client'
};

const optionsDocumentation = [
  '   - `skipAudit` : if set to true the audit log tests are skipped'
];

const _ = require('lodash');
const fs = require('fs');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const base64Encode = require('internal').base64Encode;
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  audit_server: [tu.pathForTesting('common/audit'), tu.pathForTesting('server/audit')],
  audit_client: [tu.pathForTesting('common/audit'), tu.pathForTesting('client/audit')]
};

class runBasicOnArangod extends trs.runOnArangodRunner{
  preRun() {
    // we force to use auth basic, since tests expect it!
    this.instanceManager.httpAuthOptions =  {
      'headers': {
        'Authorization': 'Basic ' + base64Encode('root:')
      }
    };
    return {state: true};
  }
}

function auditLog(onServer) {
  return function(options) {
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
        name: 'audit_' + onServer ? 'server' : 'client'
      }
    };
    
    options.auditLoggingEnabled = true;
    
    const serverOptions = Object.assign({}, tu.testServerAuthInfo, {
      'log.level': 'audit-authentication=info',
      'log.force-direct': true
    });

    print(CYAN + 'Audit log server tests...' + RESET);
    let testCases = tu.scanTestPaths(testPaths['audit_' + (onServer ? 'server' : 'client')], options);
    if (onServer) {
      return new runBasicOnArangod(options, 'audit', serverOptions).run(testCases);
    } else {
      return new trs.runInArangoshRunner(options, 'audit', serverOptions).run(testCases);
    }
  };
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['audit_server'] = auditLog(true);
  testFns['audit_client'] = auditLog(false);

  // only enable them in Enterprise Edition
  opts['skipAudit'] = !isEnterprise();

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
};
