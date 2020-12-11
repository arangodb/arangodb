/*global describe, it, afterEach */
'use strict';
const FoxxManager = require('@arangodb/foxx/manager');
const FoxxService = require('@arangodb/foxx/service');
const fs = require('fs');
const path = require('path');
const expect = require('chai').expect;
const fixtureRoot = path.resolve(require('internal').pathForTesting('common'), 'test-data');

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx self-heal cleanup', () => {
  let mount = null;
  afterEach(() => {
    if (mount !== null) {
      try {
        FoxxManager.uninstall(mount);
      } catch (e) {
        // noop
      }
    }
  });

  it('should clean up stray bundles', () => {
    const fakeBundlePath = path.resolve(FoxxService.rootBundlePath(), 'fakebundle.zip');
    fs.write(fakeBundlePath, 'gone in 30 seconds');
    expect(fs.exists(fakeBundlePath)).to.equal(true);
    FoxxManager.heal();
    expect(fs.exists(fakeBundlePath)).to.equal(false);
  });

  it('should clean up stray service folders', () => {
    const fakeServicePath = path.resolve(FoxxService.rootPath(), 'fake', 'service', 'path', 'APP');
    fs.makeDirectoryRecursive(path.resolve(fakeServicePath, 'assets'));
    fs.write(path.resolve(fakeServicePath, 'manifest.json'), 'gone in 30 seconds');
    fs.write(path.resolve(fakeServicePath, 'index.js'), 'gone in 30 seconds');
    fs.write(path.resolve(fakeServicePath, 'assets', 'lolcat.png'), 'gone in 30 seconds');
    expect(fs.exists(fakeServicePath)).to.equal(true);
    FoxxManager.heal();
    expect(fs.exists(fakeServicePath)).to.equal(false);
  });

  it('should leave legit services alone', () => {
    mount = '/fake/service/mount';
    const serviceFixture = path.resolve(fixtureRoot, 'apps', 'minimal-working-service');
    FoxxManager.install(serviceFixture, mount);
    const servicePath = FoxxService.basePath(mount);
    const bundlePath = FoxxService.bundlePath(mount);
    expect(fs.exists(servicePath)).to.equal(true);
    expect(fs.exists(bundlePath)).to.equal(true);
    FoxxManager.heal();
    expect(fs.exists(servicePath)).to.equal(true);
    expect(fs.exists(bundlePath)).to.equal(true);
  });

  it('should leave app folders in legit services alone', () => {
    mount = '/fake/service/mount';
    const serviceFixture = path.resolve(fixtureRoot, 'apps', 'service-with-app-folder');
    FoxxManager.install(serviceFixture, mount);
    const servicePath = FoxxService.basePath(mount);
    const indexPath = path.resolve(servicePath, 'app', 'index.js');
    const assetPath = path.resolve(servicePath, 'assets', 'app', 'index.js');
    expect(fs.exists(indexPath)).to.equal(true);
    expect(fs.exists(assetPath)).to.equal(true);
    FoxxManager.heal();
    expect(fs.exists(indexPath)).to.equal(true);
    expect(fs.exists(assetPath)).to.equal(true);
  });
});
