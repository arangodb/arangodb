/* global arango, describe, beforeEach, afterEach, it*/
'use strict';

const expect = require('chai').expect;
const FoxxManager = require('@arangodb/foxx/manager');
const request = require('@arangodb/request');
const util = require('@arangodb/util');
const fs = require('fs');
const internal = require('internal');
const path = require('path');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'headers');
const arangodb = require('@arangodb');
const errors = arangodb.errors;
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
  const minimalWorkingServicePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'minimal-working-service');
  const itzpapalotlPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'itzpapalotl');
  var utils = require('@arangodb/foxx/manager-utils');
  const minimalWorkingZipPath = utils.zipDirectory(minimalWorkingServicePath);

  const serviceServiceMount = '/foxx-crud-test-download';
  const serviceServicePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'service-service', 'index.js');

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
        body: {source: path.resolve(minimalWorkingServicePath, 'index.js')},
        json: true
      }
    },
    {
      name: 'localZipFile',
      request: {
        body: {source: minimalWorkingZipPath},
        json: true
      }
    },
    {
      name: 'localDir',
      request: {
        body: {source: minimalWorkingServicePath},
        json: true
      }
    },
    {
      name: 'jsBuffer',
      request: {
        body: fs.readFileSync(path.resolve(minimalWorkingServicePath, 'index.js')),
        contentType: 'application/javascript'
      }
    },
    {
      name: 'zipBuffer',
      request: {
        body: fs.readFileSync(minimalWorkingZipPath),
        contentType: 'application/zip'
      }
    },
    {
      name: 'remoteJsFile',
      request: {
        body: {source: `${origin}/_db/${db._name()}${serviceServiceMount}/js`},
        json: true
      }
    },
    {
      name: 'remoteZipFile',
      request: {
        body: {source: `${origin}/_db/${db._name()}${serviceServiceMount}/zip`},
        json: true
      }
    }
  ];
  for (const c of cases) {
    it(`installed via ${c.name} should be available`, () => {
      const installResp = request.post('/_api/foxx', Object.assign({qs: {mount}}, c.request));
      expect(installResp.status).to.equal(201);
      const resp = request.get(mount);
      expect(resp.json).to.eql({hello: 'world'});
    });

    it(`replaced via ${c.name} should be available`, () => {
      FoxxManager.install(itzpapalotlPath, mount);
      const replaceResp = request.put('/_api/foxx/service', Object.assign({qs: {mount}}, c.request));
      expect(replaceResp.status).to.equal(200);
      const resp = request.get(mount);
      expect(resp.json).to.eql({hello: 'world'});
    });

    it(`upgrade via ${c.name} should be available`, () => {
      FoxxManager.install(itzpapalotlPath, mount);
      const upgradeResp = request.patch('/_api/foxx/service', Object.assign({qs: {mount}}, c.request));
      expect(upgradeResp.status).to.equal(200);
      const resp = request.get(mount);
      expect(resp.json).to.eql({hello: 'world'});
    });
  }

  it('uninstalled should not be available', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const delResp = request.delete('/_api/foxx/service', {qs: {mount}});
    expect(delResp.status).to.equal(204);
    expect(delResp.body).to.equal('');
    const resp = request.get(mount);
    expect(resp.status).to.equal(404);
  });

  const badMainServicePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'fails-on-mount');

  it("failing on mount should not be installed", () => {
    const installResp = request.post('/_api/foxx', {
      qs: {mount},
      body: {source: badMainServicePath},
      json: true
    });
    expect(installResp.status).to.equal(400);
    expect(installResp.json).to.have.property("error", true);
    expect(installResp.json).to.have.property("errorNum", errors.ERROR_MODULE_FAILURE.code);
  });

  it("failing on mount should successfully upgrade", () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const upgradeResp = request.patch('/_api/foxx/service', {
      qs: {mount},
      body: {source: badMainServicePath},
      json: true
    });
    expect(upgradeResp.status).to.equal(200);
    const resp = request.get(mount, {headers: {accept: 'application/json'}});
    expect(resp.status).to.equal(503);
    expect(resp.json).to.have.property("error", true);
    expect(resp.json).to.have.property("errorNum", errors.ERROR_HTTP_SERVICE_UNAVAILABLE.code);
  });

  it("failing on mount should successfully replace", () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const upgradeResp = request.put('/_api/foxx/service', {
      qs: {mount},
      body: {source: badMainServicePath},
      json: true
    });
    expect(upgradeResp.status).to.equal(200);
    const resp = request.get(mount, {headers: {accept: 'application/json'}});
    expect(resp.status).to.equal(503);
    expect(resp.json).to.have.property("error", true);
    expect(resp.json).to.have.property("errorNum", errors.ERROR_HTTP_SERVICE_UNAVAILABLE.code);
  });

  const confPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-configuration');

  it('empty configuration should be available', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
  });

  it('empty non-minimal configuration should be available', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
  });

  it('empty minimal configuration should be available', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: true}});
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

  it('non-minimal configuration should be available', () => {
    FoxxManager.install(confPath, mount);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.not.have.property('current');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal configuration should be available', () => {
    FoxxManager.install(confPath, mount);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
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
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json.values).to.have.property('test1', 'test');
    expect(updateResp.json.values).to.not.have.property('test2');
    expect(updateResp.json).to.not.have.property('warnings');
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('non-minimal configuration should be available after update', () => {
    FoxxManager.install(confPath, mount);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('test1');
    expect(updateResp.json.test1).to.have.property('current', 'test');
    expect(updateResp.json.test1).to.not.have.property('warning');
    expect(updateResp.json).to.have.property('test2');
    expect(updateResp.json.test2).to.not.have.property('current');
    expect(updateResp.json.test2).to.not.have.property('warning');
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal configuration should be available after update', () => {
    FoxxManager.install(confPath, mount);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json.values).to.have.property('test1', 'test');
    expect(updateResp.json.values).to.not.have.property('test2');
    expect(updateResp.json).to.not.have.property('warnings');
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', 'test');
    expect(resp.json).to.not.have.property('test2');
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
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.have.property('test1', 'test');
    expect(replaceResp.json.values).to.not.have.property('test2');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test2', 'is required');
    const resp = request.get('/_api/foxx/configuration', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('non-minimal configuration should be available after replace', () => {
    FoxxManager.install(confPath, mount);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('test1');
    expect(replaceResp.json.test1).to.have.property('current', 'test');
    expect(replaceResp.json.test1).to.not.have.property('warning');
    expect(replaceResp.json).to.have.property('test2');
    expect(replaceResp.json.test2).to.not.have.property('current');
    expect(replaceResp.json.test2).to.have.property('warning', 'is required');
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal configuration should be available after replace', () => {
    FoxxManager.install(confPath, mount);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.have.property('test1', 'test');
    expect(replaceResp.json.values).to.not.have.property('test2');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test2', 'is required');
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', 'test');
    expect(resp.json).to.not.have.property('test2');
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

  it('non-minimal configuration should be merged after update', () => {
    FoxxManager.install(confPath, mount);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test2: 'test2'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: 'test1'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test1');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.have.property('current', 'test2');
  });

  it('minimal configuration should be merged after update', () => {
    FoxxManager.install(confPath, mount);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test2: 'test2'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: 'test1'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', 'test1');
    expect(resp.json).to.have.property('test2', 'test2');
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

  it('non-minimal configuration should be overwritten after replace', () => {
    FoxxManager.install(confPath, mount);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test2: 'test2'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', 'test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal configuration should be overwritten after replace', () => {
    FoxxManager.install(confPath, mount);
    const updateResp = request.patch('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test2: 'test2'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    const replaceResp = request.put('/_api/foxx/configuration', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: 'test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const resp = request.get('/_api/foxx/configuration', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', 'test');
    expect(resp.json).not.to.have.property('test2');
  });


  const depPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-dependencies');

  it('empty configuration should be available', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
  });

  it('empty non-minimal configuration should be available', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
  });

  it('empty minimal configuration should be available', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: true}});
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

  it('non-minimal dependencies should be available', () => {
    FoxxManager.install(depPath, mount);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.not.have.property('current');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be available', () => {
    FoxxManager.install(depPath, mount);
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({});
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
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json.values).to.have.property('test1', '/test');
    expect(updateResp.json.values).not.to.have.property('test2');
    expect(updateResp.json).to.not.have.property('warnings');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('non-minimal dependencies should be available after update', () => {
    FoxxManager.install(depPath, mount);
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('test1');
    expect(updateResp.json.test1).to.have.property('current', '/test');
    expect(updateResp.json.test1).to.not.have.property('warning');
    expect(updateResp.json).to.have.property('test2');
    expect(updateResp.json.test2).to.not.have.property('current');
    expect(updateResp.json.test2).to.not.have.property('warning');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be available after update', () => {
    FoxxManager.install(depPath, mount);
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json.values).to.have.property('test1', '/test');
    expect(updateResp.json.values).not.to.have.property('test2');
    expect(updateResp.json).to.not.have.property('warnings');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', '/test');
    expect(resp.json).to.not.have.property('test2');
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
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.have.property('test1', '/test');
    expect(replaceResp.json.values).to.not.have.property('test2');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test2', 'is required');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('non-minimal dependencies should be available after replace', () => {
    FoxxManager.install(depPath, mount);
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('test1');
    expect(replaceResp.json.test1).to.have.property('current', '/test');
    expect(replaceResp.json.test1).to.not.have.property('warning');
    expect(replaceResp.json).to.have.property('test2');
    expect(replaceResp.json.test2).to.not.have.property('current');
    expect(replaceResp.json.test2).to.have.property('warning', 'is required');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be available after replace', () => {
    FoxxManager.install(depPath, mount);
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.have.property('test1', '/test');
    expect(replaceResp.json.values).to.not.have.property('test2');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test2', 'is required');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', '/test');
    expect(resp.json).to.not.have.property('test2');
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
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.not.have.property('test1');
    expect(replaceResp.json.values).to.have.property('test2', '/test2');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test1', 'is required');
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
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json.values).to.have.property('test1', '/test1');
    expect(updateResp.json.values).to.have.property('test2', '/test2');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test1');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.have.property('current', '/test2');
  });

  it('non-minimal dependencies should be merged after update', () => {
    FoxxManager.install(depPath, mount);
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test2: '/test2'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('test1');
    expect(replaceResp.json.test1).to.have.property('warning', 'is required');
    expect(replaceResp.json).to.have.property('test2');
    expect(replaceResp.json.test2).to.have.property('current', '/test2');
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: '/test1'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('test1');
    expect(updateResp.json.test1).to.have.property('current', '/test1');
    expect(updateResp.json.test1).to.not.have.property('warning');
    expect(updateResp.json).to.have.property('test2');
    expect(updateResp.json.test2).to.have.property('current', '/test2');
    expect(updateResp.json.test2).to.not.have.property('warning');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test1');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.have.property('current', '/test2');
  });

  it('minimal dependencies should be merged after update', () => {
    FoxxManager.install(depPath, mount);
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test2: '/test2'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.have.property('test2', '/test2');
    expect(replaceResp.json.values).to.not.have.property('test1');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test1', 'is required');
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: '/test1'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json.values).to.have.property('test1', '/test1');
    expect(updateResp.json.values).to.have.property('test2', '/test2');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', '/test1');
    expect(resp.json).to.have.property('test2', '/test2');
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
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json).to.not.have.property('warnings');
    expect(updateResp.json.values).to.have.property('test2', '/test2');
    expect(updateResp.json.values).to.not.have.property('test1');
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
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.have.property('test1', '/test');
    expect(replaceResp.json.values).to.not.have.property('test2');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test2', 'is required');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('non-minimal dependencies should be overwritten after replace', () => {
    FoxxManager.install(depPath, mount);
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test2: '/test2'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('test1');
    expect(updateResp.json.test1).to.not.have.property('current');
    expect(updateResp.json.test1).to.not.have.property('warning');
    expect(updateResp.json).to.have.property('test2');
    expect(updateResp.json.test2).to.have.property('current', '/test2');
    expect(updateResp.json.test2).to.not.have.property('warning');
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: false
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('test1');
    expect(replaceResp.json.test1).to.have.property('current', '/test');
    expect(replaceResp.json.test1).to.not.have.property('warning');
    expect(replaceResp.json.test2).to.not.have.property('current');
    expect(replaceResp.json.test2).to.have.property('warning', 'is required');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: false}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1');
    expect(resp.json.test1).to.have.property('current', '/test');
    expect(resp.json).to.have.property('test2');
    expect(resp.json.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be overwritten after replace', () => {
    FoxxManager.install(depPath, mount);
    const updateResp = request.patch('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test2: '/test2'
      },
      json: true
    });
    expect(updateResp.status).to.equal(200);
    expect(updateResp.json).to.have.property('values');
    expect(updateResp.json).to.not.have.property('warnings');
    expect(updateResp.json.values).to.have.property('test2', '/test2');
    expect(updateResp.json.values).to.not.have.property('test1');
    const replaceResp = request.put('/_api/foxx/dependencies', {
      qs: {
        mount,
        minimal: true
      },
      body: {
        test1: '/test'
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    expect(replaceResp.json).to.have.property('values');
    expect(replaceResp.json.values).to.have.property('test1', '/test');
    expect(replaceResp.json.values).to.not.have.property('test2');
    expect(replaceResp.json).to.have.property('warnings');
    expect(replaceResp.json.warnings).to.have.property('test2', 'is required');
    const resp = request.get('/_api/foxx/dependencies', {qs: {mount, minimal: true}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('test1', '/test');
    expect(resp.json).to.not.have.property('test2');
  });

  it('should be downloadable', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.post('/_api/foxx/download', {
      qs: {mount},
      encoding: null
    });
    expect(resp.status).to.equal(200);
    expect(resp.headers['content-type']).to.equal('application/zip');
    expect(util.isZipBuffer(resp.body)).to.equal(true);
  });

  const readmePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-readme');

  it('should deliver the readme', () => {
    FoxxManager.install(readmePath, mount);
    const resp = request.get('/_api/foxx/readme', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.headers['content-type']).to.equal('text/plain; charset=utf-8');
    expect(resp.body).to.equal('Please read this.');
  });

  it('should indicate a missing readme', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/readme', {qs: {mount}});
    expect(resp.status).to.equal(204);
    expect(resp.body).to.equal('');
  });

  it('should provide a swagger description', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/swagger', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('swagger', '2.0');
    expect(resp.json).to.have.property('basePath', `/_db/${db._name()}${mount}`);
    expect(resp.json).to.have.property('info');
    expect(resp.json.info).to.have.property('title', 'minimal-working-manifest');
    expect(resp.json.info).to.have.property('description', '');
    expect(resp.json.info).to.have.property('version', '0.0.0');
    expect(resp.json.info).to.have.property('license');
    expect(resp.json).to.have.property('paths');
    expect(resp.json.paths).to.have.property('/');
    expect(resp.json.paths['/']).to.have.property('get');
  });

  it('list should allow excluding system services', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const withSystem = request.get('/_api/foxx');
    const withoutSystem = request.get('/_api/foxx', {qs: {excludeSystem: true}});
    const numSystemWithSystem = withSystem.json.map(service => service.mount).filter(mount => mount.startsWith('/_')).length;
    const numSystemWithoutSystem = withoutSystem.json.map(service => service.mount).filter(mount => mount.startsWith('/_')).length;
    expect(numSystemWithSystem).to.above(0);
    expect(numSystemWithSystem).to.equal(withSystem.json.length - withoutSystem.json.length);
    expect(numSystemWithoutSystem).to.equal(0);
  });

  it('should be contained in service list', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx');
    const service = resp.json.find(service => service.mount === mount);
    expect(service).to.have.property('name', 'minimal-working-manifest');
    expect(service).to.have.property('version', '0.0.0');
    expect(service).to.have.property('provides');
    expect(service.provides).to.eql({});
    expect(service).to.have.property('development', false);
    expect(service).to.have.property('legacy', false);
  });

  it('information should be returned', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/service', {qs: {mount}});
    const service = resp.json;
    expect(service).to.have.property('mount', mount);
    expect(service).to.have.property('name', 'minimal-working-manifest');
    expect(service).to.have.property('version', '0.0.0');
    expect(service).to.have.property('development', false);
    expect(service).to.have.property('legacy', false);
    expect(service).to.have.property('manifest');
    expect(service.manifest).to.be.an('object');
    expect(service).to.have.property('options');
    expect(service.options).to.be.an('object');
    expect(service).to.have.property('checksum');
    expect(service.checksum).to.be.a('string');
  });

  const scriptPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'minimal-working-setup-teardown');

  it('list of scripts should be available', () => {
    FoxxManager.install(scriptPath, mount);
    const resp = request.get('/_api/foxx/scripts', {qs: {mount}});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('setup', 'Setup');
    expect(resp.json).to.have.property('teardown', 'Teardown');
  });

  it('script should be available', () => {
    FoxxManager.install(scriptPath, mount);
    const col = `${mount}_setup_teardown`.replace(/\//, '').replace(/-/g, '_');
    expect(db._collection(col)).to.be.an('object');
    const resp = request.post('/_api/foxx/scripts/teardown', {
      qs: {mount},
      json: true
    });
    expect(resp.status).to.equal(200);
    db._flushCache();
    expect(db._collection(col)).to.equal(null);
  });

  it('non-existing script should not be available', () => {
    FoxxManager.install(scriptPath, mount);
    const resp = request.post('/_api/foxx/scripts/no', {
      qs: {mount},
      json: true
    });
    expect(resp.status).to.equal(400);
  });

  const echoPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'echo-script');

  it('should pass argv to script and return exports', () => {
    FoxxManager.install(echoPath, mount);
    const argv = {hello: 'world'};
    const resp = request.post('/_api/foxx/scripts/echo', {
      qs: {mount},
      body: argv,
      json: true
    });
    expect(resp.json).to.eql([argv]);
  });

  it('should treat array script argv like any other script argv', () => {
    FoxxManager.install(echoPath, mount);
    const argv = ['yes', 'please'];
    const resp = request.post('/_api/foxx/scripts/echo', {
      qs: {mount},
      body: argv,
      json: true
    });
    expect(resp.json).to.eql([argv]);
  });

  it('set devmode should enable devmode', () => {
    FoxxManager.install(minimalWorkingServicePath, mount);
    const resp = request.get('/_api/foxx/service', {
      qs: {mount},
      json: true
    });
    expect(resp.json.development).to.equal(false);
    const devResp = request.post('/_api/foxx/development', {
      qs: {mount},
      json: true
    });
    expect(devResp.json.development).to.equal(true);
    const respAfter = request.get('/_api/foxx/service', {
      qs: {mount},
      json: true
    });
    expect(respAfter.json.development).to.equal(true);
  });

  it('clear devmode should disable devmode', () => {
    FoxxManager.install(minimalWorkingServicePath, mount, {development: true});
    const resp = request.get('/_api/foxx/service', {
      qs: {mount},
      json: true
    });
    expect(resp.json.development).to.equal(true);
    const devResp = request.delete('/_api/foxx/development', {
      qs: {mount},
      json: true
    });
    expect(devResp.json.development).to.equal(false);
    const respAfter = request.get('/_api/foxx/service', {
      qs: {mount},
      json: true
    });
    expect(respAfter.json.development).to.equal(false);
  });

  const routes = [
    ['GET', '/_api/foxx/service'],
    ['PATCH', '/_api/foxx/service', { source: minimalWorkingZipPath }],
    ['PUT', '/_api/foxx/service', { source: minimalWorkingZipPath }],
    ['DELETE', '/_api/foxx/service'],
    ['GET', '/_api/foxx/configuration'],
    ['PATCH', '/_api/foxx/configuration'],
    ['PUT', '/_api/foxx/configuration'],
    ['GET', '/_api/foxx/dependencies'],
    ['PATCH', '/_api/foxx/dependencies'],
    ['PUT', '/_api/foxx/dependencies'],
    ['POST', '/_api/foxx/development'],
    ['DELETE', '/_api/foxx/development'],
    ['GET', '/_api/foxx/scripts'],
    ['POST', '/_api/foxx/scripts/xxx'],
    ['POST', '/_api/foxx/tests'],
    ['POST', '/_api/foxx/download'],
    ['GET', '/_api/foxx/readme'],
    ['GET', '/_api/foxx/swagger']
  ];
  for (const [method, url, body] of routes) {
    it(`should return 400 when mount is omitted for ${method} ${url}`, () => {
      const resp = request({
        method,
        url,
        body,
        json: true
      });
      expect(resp.status).to.equal(400);
    });
    it(`should return 400 when mount is unknown for ${method} ${url}`, () => {
      const resp = request({
        method,
        url,
        qs: {mount: '/dev/null'},
        body,
        json: true
      });
      expect(resp.status).to.equal(400);
    });
  }

  it('tests should run', () => {
    const testPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-tests');
    FoxxManager.install(testPath, mount);
    const resp = request.post('/_api/foxx/tests', {qs: { mount }});
    expect(resp.status).to.equal(200);
    expect(resp.json).to.have.property('stats');
    expect(resp.json).to.have.property('tests');
    expect(resp.json).to.have.property('pending');
    expect(resp.json).to.have.property('failures');
    expect(resp.json).to.have.property('passes');
  });

  it('replace on invalid mount should not be installed', () => {
    const replaceResp = request.put('/_api/foxx/service', {
      qs: {
        mount
      },
      body: {
        source: minimalWorkingZipPath
      },
      json: true
    });
    expect(replaceResp.status).to.equal(400);
    const resp = request.get(mount);
    expect(resp.status).to.equal(404);
  });

  it('replace on invalid mount should be installed when forced', () => {
    const replaceResp = request.put('/_api/foxx/service', {
      qs: {
        mount,
        force: true
      },
      body: {
        source: minimalWorkingZipPath
      },
      json: true
    });
    expect(replaceResp.status).to.equal(200);
    const resp = request.get(mount);
    expect(resp.status).to.equal(200);
    expect(resp.json).to.eql({hello: 'world'});
  });
});
