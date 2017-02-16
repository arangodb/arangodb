/* global describe, it, beforeEach, afterEach, before, after*/
'use strict';

const FoxxManager = require('@arangodb/foxx/manager');
const ArangoCollection = require('@arangodb').ArangoCollection;
const fs = require('fs');
const db = require('internal').db;
const arangodb = require('@arangodb');
const arango = require('@arangodb').arango;
const aql = arangodb.aql;
const basePath = fs.makeAbsolute(fs.join(require('internal').startupPath, 'common', 'test-data', 'apps'));
const expect = require('chai').expect;
const download = require('internal').download;
const path = require('path');
const internal = require('internal');

describe('Foxx Manager', function () {
  describe('(CRUD operation) ', function () {
    const setupTeardownApp = fs.join(basePath, 'minimal-working-setup-teardown');
    const mount = '/testmount';
    const prefix = mount.substr(1).replace(/[-.:/]/, '_');
    const setupCol = `${prefix}_setup_teardown`;

    describe('installed service', function () {
      beforeEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
        try {
          db._drop(setupCol);
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: false});
        } catch (e) {
        }
        try {
          db._drop(setupCol);
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

      it('should provide a bundle', function () {
        FoxxManager.install(setupTeardownApp, mount);
        const url = `${arango.getEndpoint().replace('tcp://', 'http://')}/_api/foxx/bundle?mount=${encodeURIComponent(mount)}`;
        const res = download(url);
        expect(res.code).to.equal(200);
        const checksum = db._query(aql`
          FOR service IN _apps
          FILTER service.mount == ${mount}
          RETURN service.checksum
        `).next();
        expect(res.headers.etag).to.equal(`"${checksum}"`);
      });

      it('should run the setup script', function () {
        FoxxManager.install(setupTeardownApp, mount);
        expect(db._collection(setupCol)).to.be.an.instanceOf(ArangoCollection);
      });

      it('with option {setup:false} should not run the setup script', function () {
        FoxxManager.install(setupTeardownApp, mount, {setup: false});
        expect(db._collection(setupCol)).to.equal(null);
      });

      it('should be available', function () {
        FoxxManager.install(setupTeardownApp, mount);
        const url = `${arango.getEndpoint().replace('tcp://', 'http://')}${mount}/test`;
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
          db._drop(setupCol);
        } catch (e) {
        }
      });

      afterEach(function () {
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
        }
        try {
          db._drop(setupCol);
        } catch (e) {
        }
      });

      it('should not conatins in _apps', function () {
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
        expect(db._collection(setupCol)).to.equal(null);
      });

      it('with option {teardown:false} should not run the teardown script', function () {
        FoxxManager.install(setupTeardownApp, mount);
        FoxxManager.uninstall(mount, {teardown: false});
        expect(db._collection(setupCol)).to.be.an.instanceOf(ArangoCollection);
      });
    });
  });
});
