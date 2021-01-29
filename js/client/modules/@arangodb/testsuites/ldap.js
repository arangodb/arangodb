/* jshint strict: false, sub: true */
/* global print */
'use strict';

// ////////////////////////////////////////////////////////////////////////////
// DISCLAIMER
//
// Copyright 2016 ArangoDB GmbH, Cologne, Germany
// Copyright 2014 triagens GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is ArangoDB GmbH, Cologne, Germany
//
// @author Heiko Kernbach
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
const tu = require('@arangodb/testutils/test-utils');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'ldap': [tu.pathForTesting('client/authentication')],
  'ldaprole': [tu.pathForTesting('client/authentication')],
  'ldapsearch': [tu.pathForTesting('client/authentication')],
  'ldapsearchplaceholder': [tu.pathForTesting('client/authentication')],
  'ldaprolesimple': [tu.pathForTesting('client/authentication')],
  'ldapsearchsimple': [tu.pathForTesting('client/authentication')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Shared conf
// //////////////////////////////////////////////////////////////////////////////

const sharedConf = {
  'server.authentication': true,
  'server.authentication-system-only': true,
  'server.jwt-secret': 'haxxmann', // hardcoded in auth.js
  'ldap.enabled': true,
  'ldap.server': '127.0.0.1',
  'ldap.port': '389',
  'ldap.binddn': 'cn=admin,dc=arangodb,dc=com',
  'ldap.bindpasswd': 'password',
  'ldap.basedn': 'dc=arangodb,dc=com',
  'ldap.superuser-role': 'adminrole',
  'javascript.allow-admin-execute': 'true',
  'server.local-authentication': 'true'
};

const prefixSuffix = {
  'ldap.prefix': 'uid=',
  'ldap.suffix': ',dc=arangodb,dc=com',
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Test Configurations
// //////////////////////////////////////////////////////////////////////////////

const ldapModeRolesConf = Object.assign({}, sharedConf, {
  // Use Roles Attribute Mode #1
  'ldap.roles-attribute-name': 'sn'
});

const ldapModeSearchConf = Object.assign({}, sharedConf, {
  // Search Mode #2 RoleSearch:
  'ldap.search-filter': 'objectClass=*',
  'ldap.search-attribute': 'uid',
  'ldap.roles-search': '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))',
  'ldap.roles-transformation': '/^cn=([^,]*),.*$/$1/'
});

const ldapModeSearchPlaceholderConf = Object.assign({}, sharedConf, {
  // Search Mode #2 RoleSearch:
  'ldap.search-filter': 'uid={USER}',
  'ldap.search-attribute': '',
  'ldap.roles-search': '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))',
  'ldap.roles-transformation': '/^cn=([^,]*),.*$/$1/'
});

const ldapModeRolesSimpleConf = Object.assign({}, ldapModeRolesConf, prefixSuffix);
const ldapModeSearchSimpleConf = Object.assign({}, ldapModeSearchConf, prefixSuffix);

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: ldap
// //////////////////////////////////////////////////////////////////////////////

const tests = {
  ldapModeRoles: {
    name: 'ldapModeRoles',
    conf: ldapModeRolesConf
  },
  ldapModeSearch: {
    name: 'ldapModeSearch',
    conf: ldapModeSearchConf
  },
  ldapModeSearchPlaceholder: {
    name: 'ldapModeSearchPlaceholder',
    conf: ldapModeSearchPlaceholderConf
  },
  ldapModeRolesPrefixSuffix: {
    name: 'ldapModeRolesPrefixSuffix',
    conf: ldapModeRolesSimpleConf
  },
  ldapModeSearchPrefixSuffix: {
    name: 'ldapModeSearchPrefixSuffix',
    conf: ldapModeSearchSimpleConf
  }
};

function parseOptions (options) {
  let toReturn = tests;

  _.each(toReturn, function (opt) {
    if (options.ldapHost) {
      opt.conf['ldap.server'] = options.ldapHost;
    }
    if (options.ldapPort) {
      opt.conf['ldap.port'] = options.ldapPort;
    }
  });
  return toReturn;
}

function authenticationLdapSearchModePrefixSuffix (options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Search Mode Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldapsearchsimple, options);

  print('Performing #4 Test: Search Mode - Simple Login Mode');
  print(opts.ldapModeSearchPrefixSuffix.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, opts.ldapModeSearchPrefixSuffix.conf);
}

function authenticationLdapSearchMode (options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Search Mode Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldapsearch, options);

  print('Performing #2 Test: Search Mode');
  print(opts.ldapModeSearch.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, opts.ldapModeSearch.conf);
}

function authenticationLdapSearchModePlaceholder (options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Search Mode Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldapsearchplaceholder, options);

  print('Performing #2 Test: Search Mode');
  print(opts.ldapModeSearch.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, opts.ldapModeSearchPlaceholder.conf);
}

function authenticationLdapRolesModePrefixSuffix (options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldaprolesimple, options);

  print('Performing #3 Test: Role Mode - Simple Login Mode');
  print(opts.ldapModeRolesPrefixSuffix.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, opts.ldapModeRolesPrefixSuffix.conf);
}

function authenticationLdapRolesMode (options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldaprole, options);

  print('Performing #1 Test: Role Mode');
  print(opts.ldapModeRoles.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, opts.ldapModeRoles.conf);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  // just a convenience wrapper for the regular tests
  testFns['ldap'] = [ 'ldaprole', 'ldapsearch', 'ldapsearchplaceholder', 'ldaprolesimple', 'ldapsearchsimple' ];

  testFns['ldaprole'] = authenticationLdapRolesMode;
  testFns['ldapsearch'] = authenticationLdapSearchMode;
  testFns['ldapsearchplaceholder'] = authenticationLdapSearchModePlaceholder;
  testFns['ldaprolesimple'] = authenticationLdapRolesModePrefixSuffix;
  testFns['ldapsearchsimple'] = authenticationLdapSearchModePrefixSuffix;

  // turn off ldap tests by default.
  opts['skipLdap'] = true;

  // only enable them in Enterprise Edition
  let version = {};
  if (global.ARANGODB_CLIENT_VERSION) {
    version = global.ARANGODB_CLIENT_VERSION(true);
    if (version['enterprise-version']) {
      opts['skipLdap'] = false;
    }
  }

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
