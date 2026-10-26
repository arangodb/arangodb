/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertFalse, arango */

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
// //////////////////////////////////////////////////////////////////////////////

// Tests that FOXX_BASE_URL restricts remote installs to the same origin
// (scheme, host and port) instead of a plain string prefix, so that a base
// URL without a trailing slash cannot be matched by lookalike hosts.

if (getOptions === true) {
  return {
    'foxx.allow-install-from-remote': 'false',
    'javascript.allow-admin-execute': 'true'
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const path = require('path');
const internal = require('internal');
const FoxxManager = require('@arangodb/foxx/manager');

const bundleServerPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'bundle-server');
const bundleServerMount = '/bundle-server';
const mount = '/test123';

/**
 * Sets (or with null: unsets) FOXX_BASE_URL in the server process and returns its new value
 */
function setServerBaseUrl (baseUrl) {
  const assignment = baseUrl === null
    ? `delete process.env.FOXX_BASE_URL;`
    : `process.env.FOXX_BASE_URL = ${JSON.stringify(baseUrl)};`;
  return arango.POST('/_admin/execute?returnBodyAsJSON=true', `
    const process = require('process');
    ${assignment}
    return process.env.FOXX_BASE_URL === undefined ? null : process.env.FOXX_BASE_URL;
  `);
}

/**
 * Calls validateInstallUrl on the server and reports whether the URL was accepted
 */
function validateOnServer (url) {
  return arango.POST('/_admin/execute?returnBodyAsJSON=true', `
    try {
      require('@arangodb/foxx/manager').validateInstallUrl(${JSON.stringify(url)});
      return { accepted: true };
    } catch (err) {
      return { accepted: false, errorNum: err.errorNum };
    }
  `);
}

/**
 * Runs check() with FOXX_BASE_URL set to baseUrl on the server, restoring the unset state afterwards
 */
function withServerBaseUrl (baseUrl, check) {
  assertEqual(baseUrl, setServerBaseUrl(baseUrl));
  try {
    check();
  } finally {
    assertEqual(null, setServerBaseUrl(null));
  }
}

function assertAccepted (baseUrl, urls) {
  withServerBaseUrl(baseUrl, () => {
    urls.forEach((url) => {
      const result = validateOnServer(url);
      assertTrue(result.accepted, { baseUrl, url, result });
    });
  });
}

function assertRejected (baseUrl, urls) {
  withServerBaseUrl(baseUrl, () => {
    urls.forEach((url) => {
      const result = validateOnServer(url);
      assertFalse(result.accepted, { baseUrl, url, result });
      assertEqual(errors.ERROR_FORBIDDEN.code, result.errorNum, { baseUrl, url, result });
    });
  });
}

/**
 * The http(s) origin of the server under test, without a trailing slash
 */
function serverOrigin () {
  return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/\/+$/, '');
}

function uninstallQuietly (mountPoint) {
  try {
    FoxxManager.uninstall(mountPoint, { force: true });
  } catch (err) {}
}

function validateInstallUrlSuite () {
  return {
    testBaseUrlWithoutTrailingSlashRejectsLookalikeHosts: function () {
      assertRejected('https://pkg.internal', [
        'https://pkg.internal.evil.com/x.zip',
        'https://pkg.internalevil.com/x.zip',
        'https://pkg.internal-evil.com/x.zip',
        'https://pkg.internal@evil.com/x.zip',
        'https://evil.com/pkg.internal/x.zip',
        'https://evil.com/?https://pkg.internal/x.zip',
        'https://evil.com/#https://pkg.internal/x.zip'
      ]);
    },

    testBaseUrlWithTrailingSlashRejectsOtherHosts: function () {
      assertRejected('https://pkg.internal/', [
        'https://pkg.internal.evil.com/x.zip',
        'https://evil.com/pkg.internal/x.zip',
        'https://pkg.internal@evil.com/x.zip'
      ]);
    },

    testBaseUrlRejectsDifferentScheme: function () {
      assertRejected('https://pkg.internal', ['http://pkg.internal/x.zip']);
      assertRejected('http://pkg.internal', ['https://pkg.internal/x.zip']);
    },

    testBaseUrlRejectsDifferentPort: function () {
      assertRejected('https://pkg.internal', [
        'https://pkg.internal:8443/x.zip',
        'https://pkg.internal:80/x.zip'
      ]);
      assertRejected('https://pkg.internal:8443', [
        'https://pkg.internal/x.zip',
        'https://pkg.internal:443/x.zip',
        'https://pkg.internal:9443/x.zip'
      ]);
    },

    testBaseUrlAcceptsSameOrigin: function () {
      assertAccepted('https://pkg.internal', [
        'https://pkg.internal/x.zip',
        'https://pkg.internal/deep/path/x.zip',
        'https://pkg.internal/x.zip?token=abc',
        'https://pkg.internal:443/x.zip',
        'https://PKG.INTERNAL/x.zip',
        'https://user:secret@pkg.internal/x.zip'
      ]);
    },

    testBaseUrlAcceptsSameOriginRegardlessOfTrailingSlashOrPath: function () {
      assertAccepted('https://pkg.internal/', ['https://pkg.internal/x.zip', 'https://pkg.internal:443/x.zip']);
      assertAccepted('https://pkg.internal/foxx/', ['https://pkg.internal/foxx/x.zip']);
      assertAccepted('https://pkg.internal:443/', ['https://pkg.internal/x.zip']);
      assertAccepted('http://pkg.internal:80', ['http://pkg.internal/x.zip']);
      assertAccepted('https://pkg.internal:8443', ['https://pkg.internal:8443/x.zip']);
    },

    testBaseUrlAcceptsIpv6Origin: function () {
      assertAccepted('http://[::1]:8529', ['http://[::1]:8529/x.zip']);
      assertRejected('http://[::1]:8529', ['http://[::1]:8530/x.zip', 'http://[::2]:8529/x.zip']);
    },

    testRejectsInstallUrlThatDoesNotParse: function () {
      assertRejected('https://pkg.internal', [
        'https://',
        'https:///x.zip',
        'https:/x.zip',
        'https:x.zip',
        'not a url',
        ''
      ]);
    },

    testRejectsEverythingWhenBaseUrlDoesNotParse: function () {
      ['not a url', 'pkg.internal', 'https://', 'https:/pkg.internal'].forEach((baseUrl) => {
        assertRejected(baseUrl, ['https://pkg.internal/x.zip', 'https://github.com/foo/bar/archive/master.zip']);
      });
    },

    testWithoutBaseUrlOnlyGithubIsAccepted: function () {
      assertEqual(null, setServerBaseUrl(null));
      [
        'https://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip',
        'http://github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip',
        'https://www.github.com/arangodb-foxx/demo-itzpapalotl/archive/refs/heads/master.zip'
      ].forEach((url) => {
        assertTrue(validateOnServer(url).accepted, url);
      });
      [
        'https://github.com.evil/foo/bar',
        'https://some.deceptive.github.com.site/foo/bar',
        'https://pkg.internal/x.zip'
      ].forEach((url) => {
        const result = validateOnServer(url);
        assertFalse(result.accepted, url);
        assertEqual(errors.ERROR_FORBIDDEN.code, result.errorNum, url);
      });
    }
  };
}

function installFlowSuite () {
  return {
    tearDown: function () {
      uninstallQuietly(mount);
      uninstallQuietly(bundleServerMount);
      setServerBaseUrl(null);
    },

    testInstallFromLookalikeHostIsForbiddenOnAllApis: function () {
      withServerBaseUrl('https://pkg.internal', () => {
        const url = 'https://pkg.internal.evil.com/x.zip';

        const viaFoxxApi = arango.POST(`/_api/foxx?mount=${mount}`, { source: url });
        assertTrue(viaFoxxApi.error, viaFoxxApi);
        assertEqual(403, viaFoxxApi.code);
        assertEqual(errors.ERROR_FORBIDDEN.code, viaFoxxApi.errorNum);

        const viaLegacyApi = arango.POST('/_admin/foxx/install', { appInfo: url, mount });
        assertTrue(viaLegacyApi.error, viaLegacyApi);
        assertEqual(403, viaLegacyApi.code);
        assertEqual(errors.ERROR_FORBIDDEN.code, viaLegacyApi.errorNum);

        const viaAardvark = arango.PUT(`/_admin/aardvark/foxxes/url?mount=${mount}`, { url });
        assertTrue(viaAardvark.error, viaAardvark);
        assertEqual(403, viaAardvark.code);
      });
    },

    testInstallFromBaseUrlWithoutTrailingSlashWorks: function () {
      FoxxManager.install(bundleServerPath, bundleServerMount);
      const origin = serverOrigin();
      withServerBaseUrl(origin, () => {
        const res = arango.POST(`/_api/foxx?mount=${mount}`, { source: `${origin}${bundleServerMount}/bundle` });
        assertFalse(res.error, res);
        assertEqual('bundle-server', res.name, res);
        assertEqual(mount, res.mount, res);
      });
    },

    testInstallFromBaseUrlWithTrailingSlashWorks: function () {
      FoxxManager.install(bundleServerPath, bundleServerMount);
      const origin = serverOrigin();
      withServerBaseUrl(`${origin}/`, () => {
        const res = arango.POST(`/_api/foxx?mount=${mount}`, { source: `${origin}${bundleServerMount}/bundle` });
        assertFalse(res.error, res);
        assertEqual('bundle-server', res.name, res);
      });
    }
  };
}

jsunity.run(validateInstallUrlSuite);
jsunity.run(installFlowSuite);
return jsunity.done();
