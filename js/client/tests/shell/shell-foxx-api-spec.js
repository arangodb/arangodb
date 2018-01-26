/* global arango, describe, beforeEach, afterEach, it*/
'use strict';

const expect = require('chai').expect;
const FoxxManager = require('@arangodb/foxx/manager');
const request = require('@arangodb/request');
const util = require('@arangodb/util');
const fs = require('fs');
const internal = require('internal');
const path = require('path');
const basePath = path.resolve(internal.startupPath, 'common', 'test-data', 'apps', 'headers');
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

describe('Foxx service', () => {
  const mount = '/foxx-crud-test';
  const basePath = path.resolve(internal.startupPath, 'common', 'test-data', 'apps', 'minimal-working-service');
  const itzPath = path.resolve(internal.startupPath, 'common', 'test-data', 'apps', 'itzpapalotl');
  var utils = require('@arangodb/foxx/manager-utils');
  const servicePath = utils.zipDirectory(basePath);

  const serviceServiceMount = '/foxx-crud-test-download';
  const serviceServicePath = path.resolve(internal.startupPath, 'common', 'test-data', 'apps', 'service-service', 'index.js');

  beforeEach(() => {
    FoxxManager.install(serviceServicePath, serviceServiceMount);
  });

  afterEach(() => {
    try {
      FoxxManager.uninstall(serviceServiceMount, {force: true});
    } catch (e) {}
  });

  afterEach(() => {
    try {
      FoxxManager.uninstall(mount, {force: true});
    } catch (e) {}
  });

  const cases = [
    {
      name: 'localJsFile',
      request: {
        qs: {
          mount
        },
        body: {
          source: path.resolve(basePath, 'index.js')
        },
        json: true
      }
    },
    {
      name: 'localZipFile',
      request: {
        qs: {
          mount
        },
        body: {
          source: servicePath
        },
        json: true
      }
    },
    {
      name: 'localDir',
      request: {
        qs: {
          mount
        },
        body: {
          source: basePath
        },
        json: true
      }
    },
    {
      name: 'jsBuffer',
      request: {
        qs: {
          mount
        },
        body: fs.readFileSync(path.resolve(basePath, 'index.js')),
        contentType: 'application/javascript'
      }
    },
    {
      name: 'zipBuffer',
      request: {
        qs: {
          mount
        },
        body: fs.readFileSync(servicePath),
        contentType: 'application/zip'
      }
    },
    {
      name: 'remoteJsFile',
      request: {
        qs: {
          mount
        },
        body: {
          source: `${origin}/_db/${db._name()}${serviceServiceMount}/js`
        },
        json: true
      }
    },
    {
      name: 'remoteZipFile',
      request: {
        qs: {
          mount
        },
        body: {
          source: `${origin}/_db/${db._name()}${serviceServiceMount}/zip`
        },
        json: true
      }
    }
  ];
  for (const c of cases) {
    it(`installed via ${c.name} should be available`, () => {
      const installResp = request.post('/_api/foxx', c.request);
      expect(installResp.status).to.equal(201);
      const resp = request.get(c.request.qs.mount);
      expect(resp.json).to.eql({hello: 'world'});
    });

    it(`replaced via ${c.name} should be available`, () => {
      FoxxManager.install(itzPath, c.request.qs.mount);
      const replaceResp = request.put('/_api/foxx/service', c.request);
      expect(replaceResp.status).to.equal(200);
      const resp = request.get(c.request.qs.mount);
      expect(resp.json).to.eql({hello: 'world'});
    });

    it(`upgrade via ${c.name} should be available`, () => {
      FoxxManager.install(itzPath, c.request.qs.mount);
      const upgradeResp = request.patch('/_api/foxx/service', c.request);
      expect(upgradeResp.status).to.equal(200);
      const resp = request.get(c.request.qs.mount);
      expect(resp.json).to.eql({hello: 'world'});
    });
  }

  it('uninstalled should not be available', () => {
    FoxxManager.install(basePath, mount);
    const delResp = request.delete('/_api/foxx/service', {qs: {mount}});
    expect(delResp.status).to.equal(204);
    expect(delResp.body).to.equal('');
    const resp = request.get(mount);
    expect(resp.status).to.equal(404);
  });

  const confPath = path.resolve(internal.startupPath, 'common', 'test-data', 'apps', 'with-configuration');

  it('empty configuration should be available', () => {
    FoxxManager.install(basePath, mount);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
  });

  it('configuration should be available', () => {
    FoxxManager.install(confPath, mount);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.not.have.property('current');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('configuration should be available after update', () => {
    FoxxManager.install(confPath, mount);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('configuration should be available after replace', () => {
    FoxxManager.install(confPath, mount);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('configuration should be merged after update', () => {
    FoxxManager.install(confPath, mount);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount
      },
      body: {
        test2: 'test2'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount
      },
      body: {
        test1: 'test1'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test1');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.have.property('current', 'test2');
  });

  it('configuration should be overwritten after replace', () => {
    FoxxManager.install(confPath, mount);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount
      },
      body: {
        test2: 'test2'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  const depPath = path.resolve(internal.startupPath, 'common', 'test-data', 'apps', 'with-dependencies');

  it('empty configuration should be available', () => {
    FoxxManager.install(basePath, mount);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
  });

  it('dependencies should be available', () => {
    FoxxManager.install(depPath, mount);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.not.have.property('current');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('dependencies should be available after update', () => {
    FoxxManager.install(depPath, mount);
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('dependencies should be available after replace', () => {
    FoxxManager.install(depPath, mount);
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('dependencies should be merged after update', () => {
    FoxxManager.install(depPath, mount);
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount
      },
      body: {
        test2: '/test2'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount
      },
      body: {
        test1: '/test1'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test1');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.have.property('current', '/test2');
  });

  it('dependencies should be overwritten after replace', () => {
    FoxxManager.install(depPath, mount);
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount
      },
      body: {
        test2: '/test2'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('should be downloadable', () => {
    FoxxManager.install(basePath, mount);
    const resp = request.post('/_api/foxx/download', {
      qs: {mount},
      encoding: null
    });
    expect(resp.status).to.equal(200);
    expect(resp.headers['content-type']).to.equal('application/zip');
    expect(util.isZipBuffer(resp.body)).to.equal(true);
  });

  it('list should allow excluding system services', () => {
    FoxxManager.install(basePath, mount);
    const withSystem = request.get('/_api/foxx');
    const withoutSystem = request.get('/_api/foxx', {qs: {excludeSystem: true}});
    const numSystemWithSystem = withSystem.json.map(service => service.mount).filter(mount => mount.startsWith('/_')).length;
    const numSystemWithoutSystem = withoutSystem.json.map(service => service.mount).filter(mount => mount.startsWith('/_')).length;
    expect(numSystemWithSystem).to.above(0);
    expect(numSystemWithSystem).to.equal(withSystem.json.length - withoutSystem.json.length);
    expect(numSystemWithoutSystem).to.equal(0);
  });

  it('should be contained in service list', () => {
    FoxxManager.install(basePath, mount);
    const resp = request.get('/_api/foxx');
    const service = resp.json.find(service => service.mount === mount);
    expect(service).to.have.property('name', 'minimal-working-manifest');
    expect(service).to.have.property('version', '0.0.0');
    expect(service).to.have.property('provides');
    expect(service.provides).to.eql({});
    expect(service).to.have.property('development', false);
    expect(service).to.have.property('legacy', false);
  });
});
