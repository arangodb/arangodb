/* global describe, it, beforeEach, afterEach */
'use strict';

const FoxxManager = require('@arangodb/foxx/manager');
const ArangoCollection = require('@arangodb').ArangoCollection;
const fs = require('fs');
const db = require('internal').db;
const arangodb = require('@arangodb');
const arango = require('@arangodb').arango;
const aql = arangodb.aql;
const basePath = fs.makeAbsolute(fs.join(require('internal').pathForTesting('common'), 'test-data', 'apps'));
const expect = require('chai').expect;
const download = require('internal').download;
const origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:');

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx Manager', function () {
  describe('(CRUD operation) ', function () {
    const setupTeardownApp = fs.join(basePath, 'minimal-working-setup-teardown');
    const setupApp = fs.join(basePath, 'minimal-working-setup');
    const mount = '/testmount';
    const prefix = mount.substr(1).replace(/[-.:/]/, '_');
    const setupTeardownCol = `${prefix}_setup_teardown`;
    const setupCol = `${prefix}_setup`;

    describe('installed service', function () {
      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
        try {
          db._drop(setupTeardownCol);
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: false});
        } catch (e) {
        }
        try {
          db._drop(setupTeardownCol);
        } catch (e) {
        }

        try {
          // some tests create these:
          db._drop('testmount_setup');
        } catch (e) {
        }
      });

      it('should contains in _apps', function () {
        FoxxManager.install(setupTeardownApp, mount);
        const checksum = db._query(aql`
          FOR service IN _apps
          FILTER service.mount == ${mount}
          RETURN service.checksum
        `).next();
        expect(checksum).to.be.a('string');
        expect(checksum).not.to.be.empty;
      });

      it('should run the setup script', function () {
        FoxxManager.install(setupTeardownApp, mount);
        expect(db._collection(setupTeardownCol)).to.be.an.instanceOf(ArangoCollection);
      });

      it('with option {setup:false} should not run the setup script', function () {
        FoxxManager.install(setupTeardownApp, mount, {setup: false});
        expect(db._collection(setupTeardownCol)).to.equal(null);
      });

      it('should be available', function () {
        FoxxManager.install(setupTeardownApp, mount);
        const url = `${origin}${mount}/test`;
        const res = download(url);
        expect(res.code).to.equal(200);
        expect(res.body).to.equal('true');
      });
    });

    describe('uninstalled service', function () {
      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
        try {
          db._drop(setupTeardownCol);
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
        try {
          db._drop(setupTeardownCol);
        } catch (e) {
        }
        try {
          // some tests create these:
          db._drop('testmount_setup');
        } catch (e) {
        }
        try {
          // some tests create these:
          db._drop('testmount_setup');
        } catch (e) {
        }
      });

      it('should not contains in _apps', function () {
        FoxxManager.install(setupTeardownApp, mount);
        FoxxManager.uninstall(mount);
        const s = db._query(aql`
          FOR service IN _apps
          FILTER service.mount == ${mount}
          RETURN service
        `).toArray();
        expect(s.length).to.equal(0);
      });

      it('should run the teardown script', function () {
        FoxxManager.install(setupTeardownApp, mount);
        FoxxManager.uninstall(mount);
        expect(db._collection(setupTeardownCol)).to.equal(null);
      });

      it('with option {teardown:false} should not run the teardown script', function () {
        FoxxManager.install(setupTeardownApp, mount);
        FoxxManager.uninstall(mount, {teardown: false});
        expect(db._collection(setupTeardownCol)).to.be.an.instanceOf(ArangoCollection);
      });
    });

    describe('replaced service', function () {
      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
        try {
          db._drop(setupTeardownCol);
        } catch (e) {
        }
        try {
          FoxxManager.install(setupTeardownApp, mount);
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
        try {
          db._drop(setupTeardownCol);
        } catch (e) {
        }
        try {
          // some tests create these:
          db._drop('testmount_setup');
        } catch (e) {
        }
      });

      it('should update it checksum in _apps', function () {
        const q = aql`
          FOR service IN _apps
          FILTER service.mount == ${mount}
          RETURN service.checksum
        `;
        const oldChecksum = db._query(q).toArray()[0];
        FoxxManager.replace(setupApp, mount);
        const newChecksum = db._query(q).toArray()[0];
        expect(newChecksum).to.not.be.equal(oldChecksum);
      });

      it('should run the old teardown and new setup script', function () {
        expect(db._collection(setupTeardownCol)).to.be.an.instanceOf(ArangoCollection);
        FoxxManager.replace(setupApp, mount);
        expect(db._collection(setupTeardownCol)).to.equal(null);
        expect(db._collection(setupCol)).to.be.an.instanceOf(ArangoCollection);
      });

      it('with option {teardown:false} should not run the teardown script', function () {
        FoxxManager.replace(setupApp, mount, {teardown: false});
        expect(db._collection(setupTeardownCol)).to.be.an.instanceOf(ArangoCollection);
        expect(db._collection(setupCol)).to.be.an.instanceOf(ArangoCollection);
      });
    });

    describe('installed service with malformed setup', function () {
      const malformedSetupApp = fs.join(basePath, 'malformed-setup-file');

      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: false});
        } catch (e) {
        }
        try {
          // some tests create these:
          db._drop('testmount_setup');
        } catch (e) {
        }
      });

      it('should not contains in _apps', function () {
        try {
          FoxxManager.install(malformedSetupApp, mount);
        } catch (e) {
        }
        const size = db._query(aql`
          FOR service IN _apps
          FILTER service.mount == ${mount}
          RETURN service.checksum
        `).toArray().length;
        expect(size).to.equal(0);
      });

      it('should not be available', function () {
        try {
          FoxxManager.install(malformedSetupApp, mount);
        } catch (e) {
        }
        const url = `${origin}${mount}/test`;
        const res = download(url);
        expect(res.code).to.equal(404);
      });
    });

    describe('installed service with malformed setup path', function () {
      const malformedSetupApp = fs.join(basePath, 'malformed-setup-path');

      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: false});
        } catch (e) {
        }
      });

      it('should not contains in _apps', function () {
        try {
          FoxxManager.install(malformedSetupApp, mount);
        } catch (e) {
        }
        const size = db._query(aql`
          FOR service IN _apps
          FILTER service.mount == ${mount}
          RETURN service.checksum
        `).toArray().length;
        expect(size).to.equal(0);
      });

      it('should not be available', function () {
        try {
          FoxxManager.install(malformedSetupApp, mount);
        } catch (e) {
        }
        const url = `${origin}${mount}/test`;
        const res = download(url);
        expect(res.code).to.equal(404);
      });
    });

    describe('installed service with broken setup', function () {
      const malformedSetupApp = fs.join(basePath, 'broken-setup-file');

      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: false});
        } catch (e) {
        }
        try {
          // some tests create these:
          db._drop('testmount_setup');
        } catch (e) {
        }
      });

      it('should contains in _apps', function () {
        try {
          FoxxManager.install(malformedSetupApp, mount);
        } catch (e) {
        }
        const size = db._query(aql`
          FOR service IN _apps
          FILTER service.mount == ${mount}
          RETURN service.checksum
        `).toArray().length;
        expect(size).to.equal(0);
      });

      it('should not be available', function () {
        try {
          FoxxManager.install(malformedSetupApp, mount);
        } catch (e) {
        }
        const url = `${origin}${mount}/test`;
        const res = download(url);
        expect(res.code).to.equal(404);
      });
    });

    describe('service with heal during setup', function () {
      // Regression test: There was a race condition during foxx service install
      // when selfHeal() ran after the service files were put in their final
      // location and before the service was written to _apps.
      // This service calls selfHeal() during setup to simulate that case.
      const setupHealApp = fs.join(basePath, 'heal-during-setup');
      const setupMinimalApp = fs.join(basePath, 'minimal-working-service');

      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: false});
        } catch (e) {
        }
      });

      it('should be available after install', function () {
        FoxxManager.install(setupHealApp, mount);
        const url = origin + mount;
        const res = download(url);
        expect(res.code).to.equal(200);
        expect(res.body).to.equal('true');
      });

      it('should be available after replace', function () {
        { // set up some service
          FoxxManager.install(setupMinimalApp, mount);
          const url = origin + mount;
          const res = download(url);
          expect(res.code).to.equal(200);
          expect(JSON.parse(res.body)).to.deep.equal({hello: 'world'});
        }

        { // replace it and call heal() during setup
          FoxxManager.replace(setupHealApp, mount);
          const url = origin + mount;
          const res = download(url);
          expect(res.code).to.equal(200);
          expect(res.body).to.equal('true');
        }
      });

      it('should be available after upgrade', function () {
        { // set up some service
          FoxxManager.install(setupMinimalApp, mount);
          const url = origin + mount;
          const res = download(url);
          expect(res.code).to.equal(200);
          expect(JSON.parse(res.body)).to.deep.equal({hello: 'world'});
        }

        { // upgrade it and call heal() during setup
          FoxxManager.upgrade(setupHealApp, mount);
          const url = origin + mount;
          const res = download(url);
          expect(res.code).to.equal(200);
          expect(res.body).to.equal('true');
        }
      });
    });
  });
});
