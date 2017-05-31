/* global arango, describe, beforeEach, afterEach, it*/
'use strict';

const expect = require('chai').expect;
const FoxxManager = require('@arangodb/foxx/manager');
const request = require('@arangodb/request');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.startupPath, 'common', 'test-data', 'apps', 'headers'));
const arangodb = require('@arangodb');
const db = arangodb.db;
const aql = arangodb.aql;
var origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');

describe('FoxxApi commit', function () {
  const mount = '/test';

  beforeEach(function () {
    try {
      FoxxManager.uninstall(mount, {force: true});
    } catch (e) {}
    FoxxManager.install(basePath, mount);
  });

  afterEach(function () {
    try {
      FoxxManager.uninstall(mount, {force: true});
    } catch (e) {}
  });

  it('should fix missing service definition', function () {
    db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        REMOVE service IN _apps
    `);
    let result = request.get('/test/header-echo', { headers: { origin } });
    expect(result.statusCode).to.equal(404);

    result = request.post('/_api/foxx/commit');
    expect(result.statusCode).to.equal(204);

    result = request.get('/test/header-echo', { headers: { origin } });
    expect(result.statusCode).to.equal(200);
    expect(result.headers['access-control-allow-origin']).to.equal(origin);
  });

  it('should fix missing checksum', function () {
    db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        UPDATE service WITH { checksum: null } IN _apps
    `);
    let checksum = db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        RETURN service.checksum
    `).next();
    expect(checksum).to.equal(null);

    const result = request.post('/_api/foxx/commit');
    expect(result.statusCode).to.equal(204);

    checksum = db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        RETURN service.checksum
    `).next();
    expect(checksum).to.not.equal(null);
  });

  it('without param replace should not fix wrong checksum', function () {
    db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        UPDATE service WITH { checksum: '1234' } IN _apps
    `);
    let checksum = db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        RETURN service.checksum
    `).next();
    expect(checksum).to.equal('1234');

    const result = request.post('/_api/foxx/commit');
    expect(result.statusCode).to.equal(204);

    checksum = db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        RETURN service.checksum
    `).next();
    expect(checksum).to.equal('1234');
  });

  it('with param replace should fix wrong checksum', function () {
    db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        UPDATE service WITH { checksum: '1234' } IN _apps
    `);
    let checksum = db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        RETURN service.checksum
    `).next();
    expect(checksum).to.equal('1234');

    const result = request.post('/_api/foxx/commit', { qs: { replace: true } });
    expect(result.statusCode).to.equal(204);

    checksum = db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        RETURN service.checksum
    `).next();
    expect(checksum).to.not.equal('1234');
  });

  it('should fix missing bundle', function () {
    db._query(aql`
      FOR bundle IN _appbundles
        REMOVE bundle IN _appbundles
    `);
    let bundles = db._query(aql`
      FOR bundle IN _appbundles
        RETURN 1
    `).toArray().length;
    expect(bundles).to.equal(0);

    const result = request.post('/_api/foxx/commit', { qs: { replace: true } });
    expect(result.statusCode).to.equal(204);

    bundles = db._query(aql`
      FOR bundle IN _appbundles
        RETURN 1
    `).toArray().length;
    expect(bundles).to.equal(1);
  });
});
