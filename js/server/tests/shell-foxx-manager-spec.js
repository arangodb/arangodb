/*global describe, beforeEach, afterEach, it, expect*/
'use strict';

var FoxxManager = require("org/arangodb/foxx/manager");
var fs = require("fs");
var internal = require('internal');
var basePath = fs.makeAbsolute(fs.join(internal.startupPath, "common", "test-data", "apps"));

describe("Foxx Manager", function () {
  describe("when manifest changes", function () {
    var mount;

    beforeEach(function () {
      mount = "/unittest/clobbered-manifest";
      FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
    });

    afterEach(function () {
      FoxxManager.uninstall(mount, {force: true});
    });

    it("doesn't mutilate mounted apps", function () {
      var deps1 = {hello: {name: 'world', required: true, version: '*'}};
      var deps2 = {clobbered: {name: 'completely', required: true, version: '*'}};

      var app = FoxxManager.lookupApp(mount);
      expect(app.manifest.dependencies).toEqual({});
      var filename = app.main.context.applicationContext.fileName('manifest.json');
      var rawJson = fs.readFileSync(filename, 'utf-8');
      var json = JSON.parse(rawJson);

      json.dependencies = deps1;
      fs.writeFileSync(filename, JSON.stringify(json));
      FoxxManager.scanFoxx(mount, {replace: true});
      app = FoxxManager.lookupApp(mount);
      expect(app.manifest.dependencies).toEqual(deps1);

      json.dependencies = deps2;
      fs.writeFileSync(filename, JSON.stringify(json));
      FoxxManager.scanFoxx(mount, {replace: true});
      app = FoxxManager.lookupApp(mount);
      expect(app.manifest.dependencies).toEqual(deps2);
    });
  });
});
