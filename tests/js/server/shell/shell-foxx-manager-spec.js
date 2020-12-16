/* global describe, beforeEach, afterEach, it*/
'use strict';

var expect = require('chai').expect;
var FoxxManager = require('org/arangodb/foxx/manager');
var fs = require('fs');
var internal = require('internal');
var basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx Manager', function () {
  describe('when manifest changes', function () {
    var mount;

    beforeEach(function () {
      mount = '/unittest/clobbered-manifest';
      FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
    });

    afterEach(function () {
      FoxxManager.uninstall(mount, {force: true});
    });

    it("doesn't mutilate mounted apps", function () {
      var deps1 = {hello: {name: 'world', required: true, version: '*', multiple: false}};
      var deps2 = {clobbered: {name: 'completely', required: true, version: '*', multiple: false}};

      var service = FoxxManager.lookupService(mount);
      expect(service.manifest.dependencies).to.eql({});
      var filename = service.main.context.fileName('manifest.json');
      var rawJson = fs.readFileSync(filename, 'utf-8');
      var json = JSON.parse(rawJson);

      json.dependencies = deps1;
      fs.writeFileSync(filename, JSON.stringify(json));
      FoxxManager.reloadInstalledService(mount);
      service = FoxxManager.lookupService(mount);
      expect(service.manifest.dependencies).to.eql(deps1);

      json.dependencies = deps2;
      fs.writeFileSync(filename, JSON.stringify(json));
      FoxxManager.reloadInstalledService(mount);
      service = FoxxManager.lookupService(mount);
      expect(service.manifest.dependencies).to.eql(deps2);
    });
  });
});
