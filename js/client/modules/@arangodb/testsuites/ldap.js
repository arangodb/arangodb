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
  'ldap': 'ldap tests'
};
const optionsDocumentation = [
  '   - `skipLdap` : if set to true the LDAP tests are skipped',
  '   - `ldapHost : Host/IP of the ldap server',
  '   - `ldapPort : Port of the ldap server'
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');
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

function ldap (options) {
  print(`DAP FQDN is: ${options.ldapHost}:${options.ldapPort} ${options.caCertFilePath}`);
  const results = { failed: 0 };
  const tests = [
    {
      name: 'ldapBasicLDAP',
      conf: {
        'server.authentication': true,
        'server.authentication-system-only': false,
        'ldap.enabled': true,
        'ldap.server': options.ldapHost,
        'ldap.port': options.ldapPort,
        'ldap.prefix': 'uid=',
        'ldap.suffix': ',dc=example,dc=com',
        'ldap.search-filter': 'objectClass=simpleSecurityObject',
        'ldap.search-attribute': 'uid',
        'ldap.permissions-attribute-name': 'description'
      },
      user: {
        name: 'fermi',
        pass: 'password'
      },
      result: {
        statusCode: 200
      }
    },
    {
      name: 'ldapBindSearchAuth',
      conf: {
        'server.authentication': true,
        'server.authentication-system-only': false,
        'ldap.enabled': true,
        'ldap.server': options.ldapHost,
        'ldap.port': options.ldapPort,
        'ldap.basedn': 'dc=example,dc=com',
        'ldap.search-filter': 'objectClass=simpleSecurityObject',
        'ldap.search-attribute': 'uid',
        'ldap.binddn': 'cn=admin,dc=example,dc=com',
        'ldap.bindpasswd': 'hallo',
        'ldap.permissions-attribute-name': 'description'
      },
      user: {
        name: 'albert',
        pass: 'password'
      },
      result: {
        statusCode: 200
      }
    },
    {
      name: 'ldapBindSearchAuthWrongUser',
      conf: {
        'server.authentication': true,
        'server.authentication-system-only': false,
        'ldap.enabled': true,
        'ldap.server': options.ldapHost,
        'ldap.port': options.ldapPort,
        'ldap.basedn': 'dc=example,dc=com',
        'ldap.search-filter': 'objectClass=simpleSecurityObject',
        'ldap.search-attribute': 'uid',
        'ldap.binddn': 'cn=admin,dc=example,dc=com',
        'ldap.bindpasswd': 'hallo',
        'ldap.permissions-attribute-name': 'description'
      },
      user: {
        name: 'werner',
        pass: 'password'
      },
      result: {
        statusCode: 401
      }
    },
    {
      name: 'ldapUrlBindSearchAuth',
      conf: {
        'server.authentication': true,
        'server.authentication-system-only': false,
        'ldap.enabled': true,
        'ldap.url': `ldap://${options.ldapHost}:${options.ldapPort}/dc=example,dc=com?uid?sub`,
        'ldap.search-filter': 'objectClass=simpleSecurityObject',
        'ldap.binddn': 'cn=admin,dc=example,dc=com',
        'ldap.bindpasswd': 'hallo',
        'ldap.permissions-attribute-name': 'description'
      },
      user: {
        name: 'fermi',
        pass: 'password'
      },
      result: {
        statusCode: 200
      }
    },
    {
      name: 'ldapUrlBindSearchTlsAuth',
      conf: {
        'server.authentication': true,
        'server.authentication-system-only': false,
        'ldap.enabled': true,
        'ldap.url': `ldap://${options.ldapHost}:${options.ldapPort}/dc=example,dc=com?uid?sub`,
        'ldap.search-filter': 'objectClass=simpleSecurityObject',
        'ldap.binddn': 'cn=admin,dc=example,dc=com',
        'ldap.bindpasswd': 'hallo',
        'ldap.permissions-attribute-name': 'description',
        'ldap.tls': true,
        'ldap.tls-cacert-file': options.caCertFilePath,
        'ldap.tls-cert-check-strategy': 'hard'

      },
      user: {
        name: 'fermi',
        pass: 'password'
      },
      result: {
        statusCode: 200
      }
    }];

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
  } // if

  if (options.cluster) {
    print('skipping LDAP tests on cluster!');
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

  for (const t of tests) {
    let cleanup = true;
    const adbInstance = pu.startInstance('tcp', options, t.conf, 'ldap');
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
      url: `${adbInstance.arangods[0].url}/_open/auth`,
      body: JSON.stringify(
        {
          username: t.user.name,
          password: t.user.pass
        })
    });

    if (t.result.statusCode !== res.statusCode) {
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

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['ldap'] = ldap;
  // defaultFns.push('ldap'); // turn off ldap tests by default
  opts['ldapHost'] = '127.0.0.1';
  opts['ldapPort'] = 3890;
  opts['caCertFilePath'] = '~/ca_cert.pem';

  // turn off ldap tests by default. only enable them in enterprise version
  opts['skipLdap'] = true;

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
