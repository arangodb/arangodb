/* global describe, beforeEach, afterEach, it*/
'use strict';

const errors = require('@arangodb').errors;
const expect = require('chai').expect;
const fs = require('fs');
const FoxxManager = require('@arangodb/foxx/manager');
const FoxxService = require('@arangodb/foxx/service');
const request = require('@arangodb/request');
const SERVICE_PATH = fs.makeAbsolute(fs.join(
  require('internal').pathForTesting('common'), 'test-data', 'apps', 'minimal-working-service'
));
const MOUNT = '/consistencytest';

let options = {
  json: true
};

if (require('internal').db._version(true)['maintainer-mode'] === 'true') {
  options.timeout = 300;
}

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx routing consistency', () => {
  beforeEach(() => {
    FoxxManager.install(SERVICE_PATH, MOUNT);
  });
  afterEach(() => {
    try {
      FoxxManager.uninstall(MOUNT, {force: true});
    } catch (e) {}
  });

  it('serves service when everything is fine', () => {
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(200);
    expect(res.json).to.eql({hello: 'world'});
  });

  it('serves ERROR_SERVICE_FILES_MISSING when bundle is missing', () => {
    fs.remove(FoxxService.bundlePath(MOUNT));
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(503);
    expect(res.json).to.have.property('errorNum', errors.ERROR_SERVICE_FILES_MISSING.code);
  });

  it('serves ERROR_SERVICE_FILES_MISSING when service folder is missing', () => {
    fs.removeDirectoryRecursive(FoxxService.basePath(MOUNT), true);
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(503);
    expect(res.json).to.have.property('errorNum', errors.ERROR_SERVICE_FILES_MISSING.code);
  });

  it('serves ERROR_SERVICE_FILES_OUTDATED when checksum is wrong', () => {
    fs.writeFileSync(FoxxService.bundlePath(MOUNT), 'keyboardcat');
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(503);
    expect(res.json).to.have.property('errorNum', errors.ERROR_SERVICE_FILES_OUTDATED.code);
  });

  it('serves ERROR_MALFORMED_MANIFEST_FILE when manifest is mangled', () => {
    fs.writeFileSync(fs.join(FoxxService.basePath(MOUNT), 'manifest.json'), 'keyboardcat');
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(503);
    expect(res.json).to.have.property('errorNum', errors.ERROR_MALFORMED_MANIFEST_FILE.code);
  });

  it('serves ERROR_SERVICE_MANIFEST_NOT_FOUND when manifest is deleted', () => {
    fs.remove(fs.join(FoxxService.basePath(MOUNT), 'manifest.json'));
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(503);
    expect(res.json).to.have.property('errorNum', errors.ERROR_SERVICE_MANIFEST_NOT_FOUND.code);
  });

  it('serves ERROR_MODULE_SYNTAX_ERROR when entry file is mangled', () => {
    fs.writeFileSync(fs.join(FoxxService.basePath(MOUNT), 'index.js'), 'const keyboardcat;');
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(503);
    expect(res.json).to.have.property('errorNum', errors.ERROR_MODULE_SYNTAX_ERROR.code);
  });

  it('serves ERROR_MODULE_NOT_FOUND when entry file is mangled', () => {
    fs.remove(fs.join(FoxxService.basePath(MOUNT), 'index.js'));
    FoxxManager.initializeFoxx();
    const res = request.get(MOUNT, options);
    expect(res.statusCode).to.equal(503);
    expect(res.json).to.have.property('errorNum', errors.ERROR_MODULE_NOT_FOUND.code);
  });
});
