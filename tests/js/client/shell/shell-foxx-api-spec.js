/* global arango, describe, beforeEach, afterEach, it VPACK_TO_V8 */
'use strict';

const expect = require('chai').expect;
var utils = require('@arangodb/foxx/manager-utils');
const FoxxManager = require('@arangodb/foxx/manager');
const request = require('@arangodb/request');
const util = require('@arangodb/util');
const fs = require('fs');
const internal = require('internal');
const path = require('path');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'headers');
const arangodb = require('@arangodb');
const arango = require('@arangodb').arango;
const errors = arangodb.errors;
const db = arangodb.db;
const aql = arangodb.aql;
var origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:');
const isVst = arango.getEndpoint().match('^vst://') !== null;

require("@arangodb/test-helper").waitForFoxxInitialized();

function loadFoxxIntoZip(path) {
  let zip = utils.zipDirectory(path);
  let content = fs.readFileSync(zip);
  fs.remove(zip);
  return {
    type: 'inlinezip',
    buffer: content
  };
}

function installFoxx(mountpoint, which) {
  let headers = {};
  let content;
  if (which.type === 'js') {
    headers['content-type'] = 'application/javascript';
    content = which.buffer;
  } else if (which.type === 'dir') {
    headers['content-type'] = 'application/zip';
    var utils = require('@arangodb/foxx/manager-utils');
    let zip = utils.zipDirectory(which.buffer);
    content = fs.readFileSync(zip);
    fs.remove(zip);
  } else if (which.type === 'inlinezip') {
    content = which.buffer;
    headers['content-type'] = 'application/zip';
  } else if (which.type === 'url') {
    content = { source: which };
  } else if (which.type === 'file') {
    content = fs.readFileSync(which.buffer);
  }
  let devmode = '';
  if (which.hasOwnProperty('devmode') && which.devmode === true) {
    devmode = '&development=true';
  }
  const crudResp = arango.POST('/_api/foxx?mount=' + mountpoint + devmode, content, headers);
  expect(crudResp).to.have.property('manifest');
}

function deleteFoxx(mountpoint) {
  const deleteResp = arango.DELETE('/_api/foxx/service?force=true&mount=' + mountpoint);
  expect(deleteResp).to.have.property('code');
  expect(deleteResp.code).to.equal(204);
  expect(deleteResp.error).to.equal(false);
}

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
    expect(db._query(aql`
      FOR service IN _apps
        FILTER service.mount == ${mount}
        REMOVE service IN _apps
    `).getExtra().stats.writesExecuted).to.equal(1);

    let body;
    let result = arango.GET('/test/header-echo');
    expect(result.code).to.equal(404);

    result = arango.POST('/_api/foxx/commit', '');
    expect(result.code).to.equal(204);
    [
      // explicitly say we want utf8-json, since the server will add it:
      { origin: origin, accept: 'application/json; charset=utf-8', test: 'first' },
      { 'accept-encoding': 'deflate', accept: 'application/json; charset=utf-8', test: "second"},
      // work around clever arangosh client, specify random content-type first:
      { accept: 'image/webp,text/html,application/x-html,*/*;q=0.8', test: "third", 'accept-encoding': 'identity'},
      { accept: 'image/webp,text/html,application/x-html,*/*;q=0.8', test: "third", 'accept-encoding': 'deflate'},
      { accept: 'image/webp,text/html,application/x-html,*/*;q=0.8', test: "third", 'accept-encoding': 'gzip'},
      { accept: 'image/webp,text/html,application/x-html,*/*;q=0.8', test: "third"},
      { accept: 'application/json; charset=utf-8', test: "fourth", "content-type": "image/jpg"}
    ].forEach(headers => {
      result = arango.GET_RAW('/test/header-echo', headers);
      expect(result.code).to.equal(200);
      if (result.headers['content-type'] === 'application/x-velocypack') {
        body = result.parsedBody;
      } else {
        body = JSON.parse(result.body);
      }
      
      Object.keys(headers).forEach(function(key) {
        let value = headers[key];
        if (key === 'origin') {
          if (arango.getEndpoint().match(/^vst:/)) {
            // GeneralServer/HttpCommTask.cpp handles this, not implemented for vst
            expect(body['origin']).to.equal(origin);
          } else {
            expect(result.headers['access-control-allow-origin']).to.equal(origin);
          }
        }
        expect(body[key]).to.equal(headers[key]);
        
      });
      if (!headers.hasOwnProperty('accept-encoding')) {
        expect(body['accept-encoding']).to.equal(undefined);
      }
    });

    // sending content-type json actually requires to post something:
    let headers = { accept: 'application/json; charset=utf-8', test: "first", "content-type": "application/json; charset=utf-8"};
    result = arango.POST_RAW('/test/header-echo', '{}', headers);
    expect(result.code).to.equal(200);
    if (result.headers['content-type'] === 'application/x-velocypack') {
      body = result.parsedBody;
    } else {
      body = JSON.parse(result.body);
    }
    
    Object.keys(headers).forEach(function(key) {
      let value = headers[key];
      
      expect(body[key]).to.equal(headers[key]);
    });
  });

  it('should deliver compressed files according to accept-encoding', function() {
    // TODO: decompress body (if) and check for its content, so double-compression can be eradicted.
    let result;

    result = arango.GET_RAW('/_db/_system/_admin/aardvark/index.html', {'accept-encoding': 'identity'});
    expect(result.body).to.contain('doctype');

    result = arango.GET_RAW('/_db/_system/_admin/aardvark/index.html', {'accept-encoding': 'deflate'});
    expect(result.body).to.contain('doctype');

    result = arango.GET_RAW('/_db/_system/_admin/aardvark/index.html', {'accept-encoding': 'gzip'});
    expect(result.body).to.be.instanceof(Buffer);

    result = arango.GET_RAW('/_db/_system/_admin/aardvark/index.html', {});
    expect(result.body).to.contain('doctype');

    result = arango.GET_RAW('/_api/version', {'accept-encoding': 'identity'});
    expect(result).to.have.property('parsedBody');

    result = arango.GET_RAW('/_api/version', {'accept-encoding': 'deflate'});
    if (!isVst) {
      // no transparent compression support in VST atm.
      expect(result.body).to.be.instanceof(Buffer);
    } else {
      expect(result).to.have.property('parsedBody');
    }      

    result = arango.GET_RAW('/_api/version', {'accept-encoding': 'gzip'});
    expect(result).to.have.property('parsedBody');

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

    const result = arango.POST('/_api/foxx/commit', '');
    expect(result.code).to.equal(204);

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

    const result = arango.POST('/_api/foxx/commit', '');
    expect(result.code).to.equal(204);

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

    const result = arango.POST('/_api/foxx/commit?replace=true', '');
    expect(result.code).to.equal(204);

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

    const result = arango.POST('/_api/foxx/commit', { qs: { replace: true } });
    expect(result.code).to.equal(204);

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
  const minimalWorkingZip = loadFoxxIntoZip(minimalWorkingServicePath);
  const minimalWorkingZipDev = {
    buffer: minimalWorkingZip.buffer,
    devmode: true,
    type: minimalWorkingZip.type
  };
  const minimalWorkingZipPath = utils.zipDirectory(minimalWorkingServicePath);
  
  const itzpapalotlPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'itzpapalotl');
  const itzpapalotlZip = loadFoxxIntoZip(itzpapalotlPath);

  const serviceServiceMount = '/foxx-crud-test-download';
  const serviceServicePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'service-service', 'index.js');
  const crudTestServiceSource = {
    type: 'js',
    buffer: fs.readFileSync(serviceServicePath)
  };
  beforeEach(() => {
    installFoxx(serviceServiceMount, crudTestServiceSource);
  });

  afterEach(() => {
    try {
      deleteFoxx(serviceServiceMount);
    } catch (e) {}
  });

  afterEach(() => {
    try {
      deleteFoxx(mount);
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
      let headers = {};
      if (c.request.hasOwnProperty('contentType')) {
        headers['content-type'] = c.request.contentType;
      }
      const installResp = arango.POST('/_api/foxx?mount=' + mount + '&foo=bar', c.request.body, headers);
      expect(installResp).to.have.property('manifest');
      const resp = arango.GET(mount);
      expect(resp).to.eql({hello: 'world'});
    });
    it(`replaced via ${c.name} should be available`, () => {
      installFoxx(mount, itzpapalotlZip);
      let headers = {};
      if (c.request.hasOwnProperty('contentType')) {
        headers['content-type'] = c.request.contentType;
      }
      const replaceResp = arango.PUT('/_api/foxx/service?mount=' + mount, c.request.body, headers);
      expect(replaceResp).to.have.property('manifest');
      const resp = arango.GET(mount);
      expect(resp).to.eql({hello: 'world'});
    });
    it(`upgrade via ${c.name} should be available`, () => {
      installFoxx(mount, itzpapalotlZip);
      let headers = {};
      if (c.request.hasOwnProperty('contentType')) {
        headers['content-type'] = c.request.contentType;
      }
      const upgradeResp = arango.PATCH('/_api/foxx/service?mount=' + mount, c.request.body, headers);
      expect(upgradeResp).to.have.property('manifest');
      const resp = arango.GET(mount);
      expect(resp).to.eql({hello: 'world'});
    });
  }

  it('uninstalled should not be available', () => {
    installFoxx(mount, minimalWorkingZip);
    const delResp = arango.DELETE('/_api/foxx/service?mount=' + mount);
    expect(delResp.code).to.equal(204);
    expect(delResp.error).to.equal(false);
    const resp = arango.GET(mount);
    expect(resp.code).to.equal(404);
  });

  const badMainServicePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'fails-on-mount');

  it('failing on mount should not be installed', () => {
    const installResp = arango.POST('/_api/foxx?mount=' + mount, { source: badMainServicePath });
    expect(installResp.code).to.equal(400);
    expect(installResp).to.have.property('error', true);
    expect(installResp).to.have.property('errorNum', errors.ERROR_MODULE_FAILURE.code);
  });

  it('failing on mount should successfully upgrade', () => {
    installFoxx(mount, minimalWorkingZip);
    const upgradeResp = arango.PATCH('/_api/foxx/service?mount=' + mount, {source: badMainServicePath});
    expect(upgradeResp).to.have.property('manifest');
    const resp = arango.GET(mount);
    expect(resp.code).to.equal(503);
    expect(resp).to.have.property('error', true);
    expect(resp).to.have.property('errorNum', errors.ERROR_HTTP_SERVICE_UNAVAILABLE.code);
  });

  it('failing on mount should successfully replace', () => {
    installFoxx(mount, minimalWorkingZip);
    const upgradeResp = arango.PUT('/_api/foxx/service?mount=' + mount, {source: badMainServicePath});
    expect(upgradeResp).to.have.property('manifest');
    const resp = arango.GET(mount);
    expect(resp.code).to.equal(503);
    expect(resp).to.have.property('error', true);
    expect(resp).to.have.property('errorNum', errors.ERROR_HTTP_SERVICE_UNAVAILABLE.code);
  });

  const confPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-configuration');

  it('empty configuration should be available', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount);
    expect(resp).to.eql({});
  });

  it('empty non-minimal configuration should be available', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=false');
    expect(resp).to.eql({});
  });

  it('empty minimal configuration should be available', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=true');
    expect(resp).to.eql({});
  });

  it('configuration should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.not.have.property('current');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal configuration should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.not.have.property('current');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal configuration should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=true');
    expect(resp).to.eql({});
  });

  it('configuration should be available after update', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount,
                                    { test1: 'test'});
    expect(updateResp).to.have.property('values');
    expect(updateResp.values).to.have.property('test1', 'test');
    expect(updateResp.values).to.not.have.property('test2');
    expect(updateResp).to.not.have.property('warnings');
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal configuration should be available after update', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount + '&minimal=false', { test1: 'test'});
    expect(updateResp).to.have.property('test1');
    expect(updateResp.test1).to.have.property('current', 'test');
    expect(updateResp.test1).to.not.have.property('warning');
    expect(updateResp).to.have.property('test2');
    expect(updateResp.test2).to.not.have.property('current');
    expect(updateResp.test2).to.not.have.property('warning');
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal configuration should be available after update', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount + '&minimal=true', {
        test1: 'test'
    });
    expect(updateResp).to.have.property('values');
    expect(updateResp.values).to.have.property('test1', 'test');
    expect(updateResp.values).to.not.have.property('test2');
    expect(updateResp).to.not.have.property('warnings');
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', 'test');
    expect(resp).to.not.have.property('test2');
  });

  it('configuration should be available after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount, {
        test1: 'test'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.have.property('test1', 'test');
    expect(replaceResp.values).to.not.have.property('test2');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test2', 'is required');
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal configuration should be available after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount + '&minimal=false', {
      test1: 'test'
    });
    expect(replaceResp).to.have.property('test1');
    expect(replaceResp.test1).to.have.property('current', 'test');
    expect(replaceResp.test1).to.not.have.property('warning');
    expect(replaceResp).to.have.property('test2');
    expect(replaceResp.test2).to.not.have.property('current');
    expect(replaceResp.test2).to.have.property('warning', 'is required');
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal configuration should be available after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount + '&minimal=true', {
      test1: 'test'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.have.property('test1', 'test');
    expect(replaceResp.values).to.not.have.property('test2');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test2', 'is required');
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', 'test');
    expect(resp).to.not.have.property('test2');
  });

  it('configuration should be merged after update', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount, {
        test2: 'test2'
    });
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount, {
        test1: 'test1'
    });
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test1');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.have.property('current', 'test2');
  });

  it('non-minimal configuration should be merged after update', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount + '&minimal=false', {
      test2: 'test2'
    });
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount + '&minimal=false', {
      test1: 'test1'
    });
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test1');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.have.property('current', 'test2');
  });

  it('minimal configuration should be merged after update', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount + '&minimal=true', {
      test2: 'test2'
    });
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount + '&minimal=true', {
      test1: 'test1'
    });
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', 'test1');
    expect(resp).to.have.property('test2', 'test2');
  });

  it('configuration should be overwritten after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount, {
      test2: 'test2'
    });
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount, {
        test1: 'test'
    });
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal configuration should be overwritten after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount + '&minimal=false', {
        test2: 'test2'
    });
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount + '&minimal=false', {
        test1: 'test'
    });
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', 'test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal configuration should be overwritten after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: confPath});
    const updateResp = arango.PATCH('/_api/foxx/configuration?mount=' + mount + '&minimal=true', {
        test2: 'test2'
    });
    const replaceResp = arango.PUT('/_api/foxx/configuration?mount=' + mount + '&minimal=true', {
        test1: 'test'
    });
    const resp = arango.GET('/_api/foxx/configuration?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', 'test');
    expect(resp).not.to.have.property('test2');
  });


  const depPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-dependencies');

  it('empty configuration should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: minimalWorkingServicePath});
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount);
    expect(resp).to.eql({});
  });

  it('empty non-minimal configuration should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: minimalWorkingServicePath});
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=false');
    expect(resp).to.eql({});
  });

  it('empty minimal configuration should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: minimalWorkingServicePath});
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=true');
    expect(resp).to.eql({});
  });

  it('dependencies should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.not.have.property('current');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal dependencies should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.not.have.property('current');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=true');
    expect(resp).to.eql({});
  });

  it('dependencies should be available after update', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount, {
        test1: '/test'
    });
    expect(updateResp).to.have.property('values');
    expect(updateResp.values).to.have.property('test1', '/test');
    expect(updateResp.values).not.to.have.property('test2');
    expect(updateResp).to.not.have.property('warnings');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal dependencies should be available after update', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount + '&minimal=false', {
        test1: '/test'
    });
    expect(updateResp).to.have.property('test1');
    expect(updateResp.test1).to.have.property('current', '/test');
    expect(updateResp.test1).to.not.have.property('warning');
    expect(updateResp).to.have.property('test2');
    expect(updateResp.test2).to.not.have.property('current');
    expect(updateResp.test2).to.not.have.property('warning');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be available after update', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount + '&minimal=true', {
        test1: '/test'
    });
    expect(updateResp).to.have.property('values');
    expect(updateResp.values).to.have.property('test1', '/test');
    expect(updateResp.values).not.to.have.property('test2');
    expect(updateResp).to.not.have.property('warnings');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', '/test');
    expect(resp).to.not.have.property('test2');
  });

  it('dependencies should be available after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount, {
        test1: '/test'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.have.property('test1', '/test');
    expect(replaceResp.values).to.not.have.property('test2');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test2', 'is required');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal dependencies should be available after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount + '&minimal=false', {
        test1: '/test'
    });
    expect(replaceResp).to.have.property('test1');
    expect(replaceResp.test1).to.have.property('current', '/test');
    expect(replaceResp.test1).to.not.have.property('warning');
    expect(replaceResp).to.have.property('test2');
    expect(replaceResp.test2).to.not.have.property('current');
    expect(replaceResp.test2).to.have.property('warning', 'is required');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be available after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount + '&minimal=true', {
        test1: '/test'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.have.property('test1', '/test');
    expect(replaceResp.values).to.not.have.property('test2');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test2', 'is required');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', '/test');
    expect(resp).to.not.have.property('test2');
  });

  it('dependencies should be merged after update', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount, {
        test2: '/test2'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.not.have.property('test1');
    expect(replaceResp.values).to.have.property('test2', '/test2');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test1', 'is required');
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount, {
        test1: '/test1'
    });
    expect(updateResp).to.have.property('values');
    expect(updateResp.values).to.have.property('test1', '/test1');
    expect(updateResp.values).to.have.property('test2', '/test2');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test1');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.have.property('current', '/test2');
  });

  it('non-minimal dependencies should be merged after update', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount + '&minimal=false', {
        test2: '/test2'
    });
    expect(replaceResp).to.have.property('test1');
    expect(replaceResp.test1).to.have.property('warning', 'is required');
    expect(replaceResp).to.have.property('test2');
    expect(replaceResp.test2).to.have.property('current', '/test2');
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount + '&minimal=false', {
        test1: '/test1'
    });
    expect(updateResp).to.have.property('test1');
    expect(updateResp.test1).to.have.property('current', '/test1');
    expect(updateResp.test1).to.not.have.property('warning');
    expect(updateResp).to.have.property('test2');
    expect(updateResp.test2).to.have.property('current', '/test2');
    expect(updateResp.test2).to.not.have.property('warning');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test1');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.have.property('current', '/test2');
  });

  it('minimal dependencies should be merged after update', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount + '&minimal=true', {
        test2: '/test2'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.have.property('test2', '/test2');
    expect(replaceResp.values).to.not.have.property('test1');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test1', 'is required');
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount + '&minimal=true', {
        test1: '/test1'
    });
    expect(updateResp).to.have.property('values');
    expect(updateResp.values).to.have.property('test1', '/test1');
    expect(updateResp.values).to.have.property('test2', '/test2');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', '/test1');
    expect(resp).to.have.property('test2', '/test2');
  });

  it('dependencies should be overwritten after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount, {
        test2: '/test2'
    });
    expect(updateResp).to.have.property('values');
    expect(updateResp).to.not.have.property('warnings');
    expect(updateResp.values).to.have.property('test2', '/test2');
    expect(updateResp.values).to.not.have.property('test1');
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount, {
        test1: '/test'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.have.property('test1', '/test');
    expect(replaceResp.values).to.not.have.property('test2');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test2', 'is required');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount);
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('non-minimal dependencies should be overwritten after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount + '&minimal=false', {
        test2: '/test2'
    });
    expect(updateResp).to.have.property('test1');
    expect(updateResp.test1).to.not.have.property('current');
    expect(updateResp.test1).to.not.have.property('warning');
    expect(updateResp).to.have.property('test2');
    expect(updateResp.test2).to.have.property('current', '/test2');
    expect(updateResp.test2).to.not.have.property('warning');
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount + '&minimal=false', {
        test1: '/test'
    });
    expect(replaceResp).to.have.property('test1');
    expect(replaceResp.test1).to.have.property('current', '/test');
    expect(replaceResp.test1).to.not.have.property('warning');
    expect(replaceResp.test2).to.not.have.property('current');
    expect(replaceResp.test2).to.have.property('warning', 'is required');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=false');
    expect(resp).to.have.property('test1');
    expect(resp.test1).to.have.property('current', '/test');
    expect(resp).to.have.property('test2');
    expect(resp.test2).to.not.have.property('current');
  });

  it('minimal dependencies should be overwritten after replace', () => {
    installFoxx(mount, {type: 'dir', buffer: depPath});
    const updateResp = arango.PATCH('/_api/foxx/dependencies?mount=' + mount + '&minimal=true', {
        test2: '/test2'
    });
    expect(updateResp).to.have.property('values');
    expect(updateResp).to.not.have.property('warnings');
    expect(updateResp.values).to.have.property('test2', '/test2');
    expect(updateResp.values).to.not.have.property('test1');
    const replaceResp = arango.PUT('/_api/foxx/dependencies?mount=' + mount + '&minimal=true', {
        test1: '/test'
    });
    expect(replaceResp).to.have.property('values');
    expect(replaceResp.values).to.have.property('test1', '/test');
    expect(replaceResp.values).to.not.have.property('test2');
    expect(replaceResp).to.have.property('warnings');
    expect(replaceResp.warnings).to.have.property('test2', 'is required');
    const resp = arango.GET('/_api/foxx/dependencies?mount=' + mount + '&minimal=true');
    expect(resp).to.have.property('test1', '/test');
    expect(resp).to.not.have.property('test2');
  });

  it('should be downloadable', () => {
    installFoxx(mount, {type: 'dir', buffer: minimalWorkingServicePath});
    const resp = arango.POST('/_api/foxx/download?mount=' + mount, '');
    // expect(resp.headers['content-type']).to.equal('application/zip');
    expect(util.isZipBuffer(resp)).to.equal(true);
  });

  const readmePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-readme');

  it('should deliver the readme', () => {
    installFoxx(mount, {type: 'dir', buffer: readmePath});
    const resp = arango.GET('/_api/foxx/readme?mount=' + mount);
    // expect(resp.headers['content-type']).to.equal('text/plain; charset=utf-8');
    expect(resp).to.equal('Please read this.');
  });

  it('should indicate a missing readme', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx/readme?mount=' + mount);
    expect(resp.code).to.equal(204);
    expect(resp.error).to.equal(false);
  });

  it('should provide a swagger description', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx/swagger?mount=' + mount);
    expect(resp).to.have.property('swagger', '2.0');
    expect(resp).to.have.property('basePath', `/_db/${db._name()}${mount}`);
    expect(resp).to.have.property('info');
    expect(resp.info).to.have.property('title', 'minimal-working-manifest');
    expect(resp.info).to.have.property('description', '');
    expect(resp.info).to.have.property('version', '0.0.0');
    expect(resp.info).to.have.property('license');
    expect(resp).to.have.property('paths');
    expect(resp.paths).to.have.property('/');
    expect(resp.paths['/']).to.have.property('get');
  });

  it('list should allow excluding system services', () => {
    installFoxx(mount, minimalWorkingZip);
    const withSystem = arango.GET('/_api/foxx');
    const withoutSystem = arango.GET('/_api/foxx?excludeSystem=true');
    const numSystemWithSystem = withSystem.map(service => service.mount).filter(mount => mount.startsWith('/_')).length;
    const numSystemWithoutSystem = withoutSystem.map(service => service.mount).filter(mount => mount.startsWith('/_')).length;
    expect(numSystemWithSystem).to.above(0);
    expect(numSystemWithSystem).to.equal(withSystem.length - withoutSystem.length);
    expect(numSystemWithoutSystem).to.equal(0);
  });

  it('should be contained in service list', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx');
    const service = resp.find(service => service.mount === mount);
    expect(service).to.have.property('name', 'minimal-working-manifest');
    expect(service).to.have.property('version', '0.0.0');
    expect(service).to.have.property('provides');
    expect(service.provides).to.eql({});
    expect(service).to.have.property('development', false);
    expect(service).to.have.property('legacy', false);
  });

  it('information should be returned', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx/service?mount=' + mount);
    const service = resp;
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
    installFoxx(mount, {type: 'dir', buffer: scriptPath});
    const resp = arango.GET('/_api/foxx/scripts?mount=' + mount);
    expect(resp).to.have.property('setup', 'Setup');
    expect(resp).to.have.property('teardown', 'Teardown');
  });

  it('script should be available', () => {
    installFoxx(mount, {type: 'dir', buffer: scriptPath});
    const col = `${mount}_setup_teardown`.replace(/\//, '').replace(/-/g, '_');
    expect(db._collection(col)).to.be.an('object');
    const resp = arango.POST('/_api/foxx/scripts/teardown?mount=' + mount, '');
    db._flushCache();
    expect(db._collection(col)).to.equal(null);
  });

  it('non-existing script should not be available', () => {
    installFoxx(mount, {type: 'dir', buffer: scriptPath});
    const resp = arango.POST('/_api/foxx/scripts/no?mount=' + mount, '');
  });

  const echoPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'echo-script');

  it('should pass argv to script and return exports', () => {
    installFoxx(mount, {type: 'dir', buffer: echoPath});
    const argv = {hello: 'world'};
    const resp = arango.POST('/_api/foxx/scripts/echo?mount=' + mount, argv);
    expect(resp).to.eql([argv]);
  });

  it('should treat array script argv like any other script argv', () => {
    installFoxx(mount, {type: 'dir', buffer: echoPath});
    const argv = ['yes', 'please'];
    const resp = arango.POST('/_api/foxx/scripts/echo?mount=' + mount, argv);
    expect(resp).to.eql([argv]);
  });

  it('set devmode should enable devmode', () => {
    installFoxx(mount, minimalWorkingZip);
    const resp = arango.GET('/_api/foxx/service?mount=' + mount);
    expect(resp.development).to.equal(false);
    const devResp = arango.POST('/_api/foxx/development?mount=' + mount, '');
    expect(devResp.development).to.equal(true);
    const respAfter = arango.GET('/_api/foxx/service?mount=' + mount);
    expect(respAfter.development).to.equal(true);
  });

  it('clear devmode should disable devmode', () => {
    installFoxx(mount, minimalWorkingZipDev);
    const resp = arango.GET('/_api/foxx/service?mount=' + mount);
    expect(resp.development).to.equal(true);
    const devResp = arango.DELETE('/_api/foxx/development?mount=' + mount);
    expect(devResp.development).to.equal(false);
    const respAfter = arango.GET('/_api/foxx/service?mount=' + mount);
    expect(respAfter.development).to.equal(false);
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
  for (const [reqFun, url, body] of routes) {
    it(`should return 400 when mount is omitted for ${reqFun} ${url}`, () => {
      let bbody = body;
      if (body === undefined) {
        bbody = '';
      }
      const resp = arango[reqFun](url, bbody);
      expect(resp.code).to.equal(400);
    });
    it(`should return 400 when mount is unknown for ${reqFun} ${url}`, () => {
      let bbody = body;
      if (body === undefined) {
        bbody = '';
      }
      const resp = arango[reqFun](url + '?mount=/dev/null', bbody);
      expect(resp.code).to.equal(400);
    });
  }

  it('tests should run', () => {
    const testPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-tests');
    FoxxManager.install(testPath, mount);
    const resp = arango.POST('/_api/foxx/tests?mount=' + mount, '');
    expect(resp).to.have.property('stats');
    expect(resp).to.have.property('tests');
    expect(resp).to.have.property('pending');
    expect(resp).to.have.property('failures');
    expect(resp).to.have.property('passes');
  });

  it('idiomatic tests reporter should return string', () => {
    const testPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-tests');
    FoxxManager.install(testPath, mount);
    const resp = arango.POST('/_api/foxx/tests?reporter=xunit&idiomatic=true&mount=' + mount, '');
    expect(resp).to.be.instanceof(Buffer);
    expect(resp.toString("utf-8").startsWith("<?xml")).to.equal(true);
  });

  it('non-idiomatic tests reporter should not return string', () => {
    const testPath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'with-tests');
    FoxxManager.install(testPath, mount);
    const resp = arango.POST('/_api/foxx/tests?reporter=xunit&idiomatic=false&mount=' + mount, '');
    expect(resp).to.be.an("array");
  });

  it('replace on invalid mount should not be installed', () => {
    const replaceResp = arango.PUT('/_api/foxx/service?mount=' + mount, {
        source: minimalWorkingZipPath
    });
    expect(replaceResp.code).to.equal(400);
    const resp = arango.GET(mount);
    expect(resp.code).to.equal(404);
  });

  it('replace on invalid mount should be installed when forced', () => {
    const replaceResp = arango.PUT('/_api/foxx/service?mount=' + mount + '&force=true', {
        source: minimalWorkingZipPath
    });
    const resp = arango.GET(mount);
    expect(resp).to.eql({hello: 'world'});
  });

});
