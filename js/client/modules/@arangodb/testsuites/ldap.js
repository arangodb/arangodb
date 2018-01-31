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
// / @author Heiko Kernbach
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'ldap': 'ldap tests'
};
const optionsDocumentation = [
  '   - `skipLdap` : if set to true the LDAP tests are skipped',
  '   - `ldapHost : Host/IP of the ldap server',
  '   - `ldapPort : Port of the ldap server'
];

const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const request = require('@arangodb/request');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: ldap
// //////////////////////////////////////////////////////////////////////////////

const tests = {
  ldapModeRoles: {
    name: 'ldapModeRoles', // TODO: needs to be renamed
    conf: {
      'server.authentication': true,
      'server.authentication-system-only': false,
      'ldap.enabled': true,
      'ldap.server': '127.0.0.1',
      'ldap.port': '389',
      'ldap.binddn': 'cn=admin,dc=arangodb,dc=com',
      'ldap.bindpasswd': 'password',
      'ldap.basedn': 'dc=arangodb,dc=com',
      'ldap.roles-attribute-name': 'sn',
      'ldap.superuser-role': 'adminrole',
      'log.level': 'ldap=trace'
    },
    user: {
      name: 'user1',
      pass: 'password',
      role: 'role1'
    }
  }
};

function parseOptions (options) {
  let toReturn = tests;

  _.each(toReturn, function (opt) {
    if (options.ldapHost) {
      opt.ldapHost = options.ldapHost;
    }
    if (options.ldapPort) {
      opt.ldapPort = options.ldapPort;
    }
  });
  return toReturn;
}

function startLdap (options) {
  const results = { failed: 0 };
  const opts = parseOptions(options);

  print(`LDAP Server is: ${opts.ldapHost}:${opts.ldapPort}`);

  if (options.skipLdap === true) {
    print('skipping LDAP tests!');
    return {
      failed: 0,
      ldap: {
        failed: 0,
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'LDAP tests...' + RESET);
  // we append one cleanup directory for the invoking logic...
  let dummyDir = fs.join(fs.getTempPath(), 'ldap_dummy');
  fs.makeDirectory(dummyDir);
  pu.cleanupDBDirectoriesAppend(dummyDir);

  if (options.cluster) {
    options['server.jwt-secret'] = 'ldap';
    options.dbServers = 2;
    options.coordinators = 2;

    for (const test of tests) {
      test.conf['server.jwt-secret'] = 'ldap';
    }
  }

  for (const t of tests) {
    let cleanup = true;
    let adbInstance = pu.startInstance('tcp', options, t.conf, 'ldap');
    if (adbInstance === false) {
      results.failed += 1;
      results[t.name] = {
        failed: 1,
        status: false,
        message: 'failed to start server!'
      };
      continue;
    }

    const res = request.post({
      url: `${adbInstance.url}/_open/auth`,
      body: JSON.stringify(
        {
          username: t.user.name,
          password: t.user.pass
        })
    });

    if (res.statusCode !== 200) {
      results.failed += 1;
      results[t.name] = {
        failed: 1,
        status: false
      };
      cleanup = false;
    } else {
      results[t.name] = {
        failed: 0,
        status: true
      };
    }
    pu.shutdownInstance(adbInstance, options);

    if (cleanup) {
      pu.cleanupLastDirectory(options);
    }
  }

  print(results);
  return results;
}

function authenticationLdapRoleClient (options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdap: {
        status: true,
        skipped: true
      }
    };
  }

  const opts = parseOptions(options);

  print(CYAN + 'Client LDAP Authentication tests...' + RESET);
  let testCases = tu.scanTestPath('js/client/tests/ldap');

  print(opts.ldapModeRoles);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, opts.ldapModeRoles.conf);
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  // testFns['ldap_start'] = startLdap;
  testFns['ldap_permissions'] = authenticationLdapRoleClient;

  // defaultFns.push('ldap'); // turn off ldap tests by default
  // turn off ldap tests by default.
  opts['skipLdap'] = true;

  // only enable them in enterprise version
  let version = {};
  if (global.ARANGODB_CLIENT_VERSION) {
    version = global.ARANGODB_CLIENT_VERSION(true);
    if (version['enterprise-version']) {
      opts['skipLdap'] = false;
    }
  }

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
