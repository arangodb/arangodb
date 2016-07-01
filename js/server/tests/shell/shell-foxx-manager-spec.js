/* global describe, beforeEach, afterEach, it*/
'use strict';

var expect = require('chai').expect;
var FoxxManager = require('org/arangodb/foxx/manager');
var fs = require('fs');
var internal = require('internal');
var basePath = fs.makeAbsolute(fs.join(internal.startupPath, 'common', 'test-data', 'apps'));

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
      var deps1 = {hello: {name: 'world', required: true, version: '*'}};
      var deps2 = {clobbered: {name: 'completely', required: true, version: '*'}};

      var app = FoxxManager.lookupService(mount);
      expect(app.manifest.dependencies).to.eql({});
      var filename = app.main.context.fileName('manifest.json');
      var rawJson = fs.readFileSync(filename, 'utf-8');
      var json = JSON.parse(rawJson);

      json.dependencies = deps1;
      fs.writeFileSync(filename, JSON.stringify(json));
      FoxxManager.scanFoxx(mount, {replace: true});
      app = FoxxManager.lookupService(mount);
      expect(app.manifest.dependencies).to.eql(deps1);

      json.dependencies = deps2;
      fs.writeFileSync(filename, JSON.stringify(json));
      FoxxManager.scanFoxx(mount, {replace: true});
      app = FoxxManager.lookupService(mount);
      expect(app.manifest.dependencies).to.eql(deps2);
    });
  });
});
