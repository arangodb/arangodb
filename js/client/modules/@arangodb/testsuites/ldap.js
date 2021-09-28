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
  '   - `ldapPort : Port of the ldap server',
  '   - `ldap2Host : Host/IP of the secondary ldap server',
  '   - `ldap2Port : Port of the secondary ldap server'
];

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');
const fs = require('fs');
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');

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
  'ldapsearchsimple': [tu.pathForTesting('client/authentication')],
  'dualldap': [tu.pathForTesting('client/ldap')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Shared conf
// //////////////////////////////////////////////////////////////////////////////

const sharedConf = {
  'server.authentication': true,
  'server.authentication-system-only': true,
  'server.jwt-secret': 'haxxmann', // hardcoded in auth.js
  'server.local-authentication': 'true',
  'javascript.allow-admin-execute': 'true',

  'ldap.enabled': true,
  'ldap.server': '127.0.0.1',
  'ldap.port': '389',
  'ldap.binddn': 'cn=admin,dc=arangodb,dc=com',
  'ldap.bindpasswd': 'password',
  'ldap.basedn': 'dc=arangodb,dc=com',
  'ldap.superuser-role': 'adminrole',

  'ldap2.enabled': false,
  'ldap2.server': '127.0.0.1',
  'ldap2.port': '389',
  'ldap2.binddn': 'cn=admin,dc=arangodb,dc=com',
  'ldap2.bindpasswd': 'password',
  'ldap2.basedn': 'dc=arangodb,dc=com',
  'ldap2.superuser-role': 'adminrole'
};

const prefixSuffix = {
  'ldap.prefix': 'uid=',
  'ldap.suffix': ',dc=arangodb,dc=com',

  'ldap2.prefix': 'uid=',
  'ldap2.suffix': ',dc=arangodb,dc=com'
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Test Configurations
// //////////////////////////////////////////////////////////////////////////////

const ldapModeRolesConf = Object.assign({}, sharedConf, {
  // Use Roles Attribute Mode #1
  'ldap.roles-attribute-name': 'sn',
  'ldap2.roles-attribute-name': 'sn'
});

const ldapModeSearchConf = Object.assign({}, sharedConf, {
  // Search Mode #2 RoleSearch:
  'ldap.search-filter': 'objectClass=*',
  'ldap.search-attribute': 'uid',
  'ldap.roles-search': '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))',
  'ldap.roles-transformation': '/^cn=([^,]*),.*$/$1/',

  'ldap2.search-filter': 'objectClass=*',
  'ldap2.search-attribute': 'uid',
  'ldap2.roles-search': '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))',
  'ldap2.roles-transformation': '/^cn=([^,]*),.*$/$1/'
});

const ldapModeSearchPlaceholderConf = Object.assign({}, sharedConf, {
  // Search Mode #2 RoleSearch:
  'ldap.search-filter': 'uid={USER}',
  'ldap.search-attribute': '',
  'ldap.roles-search': '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))',
  'ldap.roles-transformation': '/^cn=([^,]*),.*$/$1/',

  'ldap2.search-filter': 'uid={USER}',
  'ldap2.search-attribute': '',
  'ldap2.roles-search': '(&(objectClass=groupOfUniqueNames)(uniqueMember={USER}))',
  'ldap2.roles-transformation': '/^cn=([^,]*),.*$/$1/'
});

const ldapModeRolesSimpleConf = Object.assign({}, ldapModeRolesConf, prefixSuffix);
const ldapModeSearchSimpleConf = Object.assign({}, ldapModeSearchConf, prefixSuffix);

const dualLdapConf = {
  'server.authentication': true,
  'server.authentication-system-only': true,
  'server.jwt-secret': 'haxxmann', // hardcoded in auth.js
  'server.local-authentication': 'true',

  'ldap.enabled': true,
  'ldap.server': 'ldapserver1',
  'ldap.port': '389',
  'ldap.binddn': 'cn=admin,dc=arangodb,dc=com',
  'ldap.bindpasswd': 'password',
  'ldap.basedn': 'dc=arangodb,dc=com',
  'ldap.prefix': 'uid=',
  'ldap.suffix': ',dc=arangodb,dc=com',
  'ldap.roles-attribute-name': 'sn',
  'ldap.responsible-for': tu.pathForTesting('client/ldap/responsible1'),

  'ldap2.enabled': true,
  'ldap2.server': 'ldapserver2',
  'ldap2.port': '389',
  'ldap2.binddn': 'cn=admin,dc=arangodb,dc=com',
  'ldap2.bindpasswd': 'password',
  'ldap2.basedn': 'dc=arangodb,dc=com',
  'ldap2.prefix': 'uid=',
  'ldap2.suffix': ',dc=com',
  'ldap2.roles-attribute-name': 'sn',
  'ldap2.responsible-for': tu.pathForTesting('client/ldap/responsible2')
};

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
  },
  dualldap: {
    name: 'dualldap',
    conf: dualLdapConf
  },
};

function parseOptions(options, ldap2) {
  let toReturn = tests;

  _.each(toReturn, function(opt) {
    if (options.ldapHost) {
      opt.conf['ldap.server'] = options.ldapHost;
    }
    if (options.ldap2Host) {
      opt.conf['ldap2.server'] = options.ldap2Host;

      if (ldap2) {
        opt.conf['ldap.enabled'] = true;
        opt.conf['ldap2.enabled'] = true;
      }
    }
    if (options.ldapPort) {
      opt.conf['ldap.port'] = options.ldapPort;
    }
    if (options.ldap2Port) {
      opt.conf['ldap2.port'] = options.ldap2Port;
    }
  });
  return toReturn;
}

const activateFailurePoint = function(options, server, instance, custom) {
  let baseUrl = function(e) {
    return e.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  const jwt = crypto.jwtEncode(server['server.jwt-secret'], {
    "preferred_username": "root",
    "iss": "arangodb",
    "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');

  const endpoints = instance.endpoints;
  for (const endpoint of endpoints) {
    let result = request({
      method: "PUT",
      url: baseUrl(endpoint) + "/_admin/debug/failat/LdapFlakyLdap",
      auth: {
        bearer: jwt,
      },
      body: {}
    });
  }

  return {
    state: true
  };
};

function authenticationLdapSearchModePrefixSuffix(options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, true);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Search Mode Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldapsearchsimple, options);

  print('Performing #1 Test: Search Mode - Simple Login Mode');
  print(opts.ldapModeSearchPrefixSuffix.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh,
    opts.ldapModeSearchPrefixSuffix.conf, {
      postStart: activateFailurePoint
    });
}

function authenticationLdapSearchMode(options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, true);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Search Mode Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldapsearch, options);

  print('Performing #2 Test: Search Mode');
  print(opts.ldapModeSearch.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh,
    opts.ldapModeSearch.conf, {
      postStart: activateFailurePoint
    });
}

function authenticationLdapSearchModePlaceholder(options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, true);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Search Mode Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldapsearchplaceholder, options);

  print('Performing #3 Test: Search Mode');
  print(opts.ldapModeSearch.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh,
    opts.ldapModeSearchPlaceholder.conf, {
      postStart: activateFailurePoint
    });
}

function authenticationLdapRolesModePrefixSuffix(options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, true);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldaprolesimple, options);

  print('Performing #4 Test: Role Mode - Simple Login Mode');
  print(opts.ldapModeRolesPrefixSuffix.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh,
    opts.ldapModeRolesPrefixSuffix.conf, {
      postStart: activateFailurePoint
    });
}

function authenticationLdapRolesMode(options) {
  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, true);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Permission tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.ldaprole, options);

  print('Performing #5 Test: Role Mode');
  print(opts.ldapModeRoles.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh,
    opts.ldapModeRoles.conf, {
      postStart: activateFailurePoint
    });
}

function authenticationLdapTwoLdap(options) {
  // this will start a setup with two active LDAP servers

  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, false);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  print(CYAN + 'Client LDAP Failover with two ldap servers...' + RESET);
  let testCases = testPaths.dualldap.map(x => fs.join(x, "auth-dualldap.js"));

  print('Performing #6 Test: Failover - Scenario 1: two active LDAP servers');
  print(opts.dualldap.conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, opts.dualldap.conf);
}

function authenticationLdapFirstLdap(options) {
  // this will start a setup with two LDAP servers configured, one active

  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, false);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  const conf = Object.assign({}, opts.dualldap.conf, {
    'ldap2.server': 'unreachable'
  });

  print(CYAN + 'Client LDAP Failover with first active and second unreachable...' + RESET);
  let testCases = testPaths.dualldap.map(x => fs.join(x, "auth-firstldap.js"));

  print('Performing #7 Test: Failover - Scenario 2: first LDAP server active, second LDAP inactive');
  print(conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, conf);
}

function authenticationLdapSecondLdap(options) {
  // this will start a setup with two LDAP servers configured, one active

  if (options.skipLdap === true) {
    print('skipping Ldap Authentication tests!');
    return {
      authenticationLdapPermissions: {
        status: true,
        skipped: true
      }
    };
  }
  const opts = parseOptions(options, false);
  if (options.cluster) {
    options.dbServers = 2;
    options.coordinators = 2;
  }

  const conf = Object.assign({}, opts.dualldap.conf, {
    'ldap.server': 'unreachable'
  });

  print(CYAN + 'Client LDAP Failover with second active and first unreachable...' + RESET);
  let testCases = testPaths.dualldap.map(x => fs.join(x, "auth-secondldap.js"));

  print('Performing #8 Test: Failover - Scenario 3: first LDAP server inactive, second LDAP server active');
  print(conf);
  return tu.performTests(options, testCases, 'ldap', tu.runInArangosh, conf);
}

exports.setup = function(testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  // just a convenience wrapper for the regular tests
  testFns['ldap'] = ['ldaprole', 'ldapsearch', 'ldapsearchplaceholder', 'ldaprolesimple', 'ldapsearchsimple'];

  testFns['ldaprole'] = authenticationLdapRolesMode;
  testFns['ldapsearch'] = authenticationLdapSearchMode;
  testFns['ldapsearchplaceholder'] = authenticationLdapSearchModePlaceholder;
  testFns['ldaprolesimple'] = authenticationLdapRolesModePrefixSuffix;
  testFns['ldapsearchsimple'] = authenticationLdapSearchModePrefixSuffix;
  testFns['ldapdualldap'] = authenticationLdapTwoLdap;
  testFns['ldapfirstldap'] = authenticationLdapFirstLdap;
  testFns['ldapsecondldap'] = authenticationLdapSecondLdap;

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

  for (var attrname in functionsDocumentation) {
    fnDocs[attrname] = functionsDocumentation[attrname];
  }
  for (var i = 0; i < optionsDocumentation.length; i++) {
    optionsDoc.push(optionsDocumentation[i]);
  }
};
