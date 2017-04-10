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
// / @author Manuel Baesler
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'ldap': 'ldap tests',
};
const optionsDocumentation = [];

const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const fs = require('fs');
const yaml = require('js-yaml');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const download = require('internal').download;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: authentication
// //////////////////////////////////////////////////////////////////////////////

function ldap (options) {

  print(`DAP FQDN is: ${options.ldapURL}`);



  if (options.skipLdap === true) {
    print('skipping LDAP tests!');
    return {
      ldap: {
        status: true,
        skipped: true
      }
    };
  } // if

  if (options.cluster) {
    print('skipping LDAP tests on cluster!');
    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'LDAP tests...' + RESET);

  return {
    authentication: {
      status: true,
      skipped: true
    }
  };
}

exports.setup = function(testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['ldap'] = ldap;
  defaultFns.push('ldap');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}
