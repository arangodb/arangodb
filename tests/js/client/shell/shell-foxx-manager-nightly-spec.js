/* global describe, it, beforeEach, afterEach*/
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for Foxx manager
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / @author Michael Hackstein
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const FoxxManager = require('@arangodb/foxx/manager');
const ArangoCollection = require('@arangodb').ArangoCollection;
const fs = require('fs');
const db = require('internal').db;
const basePath = fs.makeAbsolute(fs.join(require('internal').pathForTesting('common'), 'test-data', 'apps'));
const arango = require('@arangodb').arango;
const origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:');
const expect = require('chai').expect;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx Manager', function () {
  describe('using different dbs', function () {
    beforeEach(function () {
      arango.reconnect(origin, '_system', 'root', '');
      try {
        db._dropDatabase('tmpFMDB');
      } catch (err) {
        // noop
      }
      try {
        db._dropDatabase('tmpFMDB2');
      } catch (err) {
        // noop
      }
      db._createDatabase('tmpFMDB');
      db._createDatabase('tmpFMDB2');
    });

    afterEach(function () {
      arango.reconnect(origin, '_system', 'root', '');
      db._dropDatabase('tmpFMDB');
      db._dropDatabase('tmpFMDB2');
    });

    it('should allow to install apps on same mount point', function () {
      const download = require('internal').download;
      arango.reconnect(origin, 'tmpFMDB', 'root', '');
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'itzpapalotl'), '/unittest');
      }).not.to.throw();
      arango.reconnect(origin, 'tmpFMDB2', 'root', '');
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), '/unittest');
      }).not.to.throw();
      db._useDatabase('_system');
      const baseUrl = origin + '/_db';
      const available = download(baseUrl + '/tmpFMDB/unittest/random');
      expect(available.code).to.equal(200);
      const unavailable = download(baseUrl + '/tmpFMDB2/unittest/random');
      expect(unavailable.code).to.equal(404);
    });
  });

  describe('upgrading', function () {
    const download = require('internal').download;
    const colSetup = 'unittest_upgrade_setup';
    const colSetupTeardown = 'unittest_upgrade_setup_teardown';
    const mount = '/unittest/upgrade';
    const setupTeardownApp = fs.join(basePath, 'minimal-working-setup-teardown');
    const setupApp = fs.join(basePath, 'minimal-working-setup');
    const url = origin + '/_db/_system' + mount + '/test';
    const brokenApp = fs.join(basePath, 'broken-controller-file');

    beforeEach(function () {
      try {
        db._drop(colSetup);
      } catch (e) {
        // noop
      }
      try {
        db._drop(colSetupTeardown);
      } catch (e) {
        // noop
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch (e) {
        // noop
      }
      FoxxManager.install(setupTeardownApp, mount);
      expect(db._collection(colSetupTeardown)).to.be.an.instanceOf(ArangoCollection);
      expect(download(url).code).to.equal(200);
    });

    afterEach(function () {
      try {
        db._drop(colSetup);
      } catch (e) {
        // noop
      }
      try {
        db._drop(colSetupTeardown);
      } catch (e) {
        // noop
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch (e) {
        // noop
      }
    });

    it('should run the setup script', function () {
      expect(db._collection(colSetup)).to.equal(null);
      FoxxManager.upgrade(setupApp, mount);
      expect(db._collection(colSetup)).to.be.an.instanceOf(ArangoCollection);
    });

    it('should not run the teardown script', function () {
      expect(db._collection(colSetupTeardown)).to.be.an.instanceOf(ArangoCollection);
      FoxxManager.upgrade(setupApp, mount);
      expect(db._collection(colSetupTeardown)).to.be.an.instanceOf(ArangoCollection);
    });

    it('should keep the old app reachable', function () {
      try {
        FoxxManager.upgrade(brokenApp, mount);
      } catch (e) {
        // noop
      }
      expect(download(url).code).to.equal(200);
    });

    it('should not execute teardown of the old app', function () {
      try {
        FoxxManager.upgrade(brokenApp, mount);
      } catch (e) {
        // noop
      }
      expect(db._collection(colSetupTeardown)).to.be.an.instanceOf(ArangoCollection);
    });
  });

  describe('replacing', function () {
    const download = require('internal').download;
    const colSetup = 'unittest_replace_setup';
    const colSetupTeardown = 'unittest_replace_setup_teardown';
    const mount = '/unittest/replace';
    const setupTeardownApp = fs.join(basePath, 'minimal-working-setup-teardown');
    const setupApp = fs.join(basePath, 'minimal-working-setup');
    const url = origin + '/_db/_system' + mount + '/test';
    const brokenApp = fs.join(basePath, 'broken-controller-file');

    beforeEach(function () {
      try {
        db._drop(colSetup);
      } catch (e) {
        // noop
      }
      try {
        db._drop(colSetupTeardown);
      } catch (e) {
        // noop
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch (e) {
        // noop
      }
      FoxxManager.install(setupTeardownApp, mount);
      expect(db._collection(colSetupTeardown)).to.be.an.instanceOf(ArangoCollection);
      expect(download(url).code).to.equal(200);
    });

    afterEach(function () {
      try {
        db._drop(colSetup);
      } catch (e) {
        // noop
      }
      try {
        db._drop(colSetupTeardown);
      } catch (e) {
        // noop
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch (e) {
        // noop
      }
    });

    it('should run the setup script', function () {
      expect(db._collection(colSetup)).to.equal(null);
      FoxxManager.replace(setupApp, mount);
      expect(db._collection(colSetup)).to.be.an.instanceOf(ArangoCollection);
    });

    it('should run the teardown script', function () {
      expect(db._collection(colSetupTeardown)).to.be.an.instanceOf(ArangoCollection);
      FoxxManager.replace(setupApp, mount);
      expect(db._collection(colSetupTeardown)).to.equal(null);
    });

    it('should make the original app unreachable', function () {
      FoxxManager.replace(setupApp, mount);
      expect(download(url).code).to.equal(404);
    });

    it('with broken app it should keep the old app reachable', function () {
      try {
        FoxxManager.replace(brokenApp, mount);
      } catch (e) {
        // noop
      }
      expect(download(url).code).to.equal(200);
    });

    it('with broken app it should not execute teardown of the old app', function () {
      try {
        FoxxManager.replace(brokenApp, mount);
      } catch (e) {
        // noop
      }
      expect(db._collection(colSetupTeardown)).to.be.an.instanceOf(ArangoCollection);
    });
  });
});
