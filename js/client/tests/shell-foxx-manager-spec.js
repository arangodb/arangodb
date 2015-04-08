/*jshint globalstrict:false, strict:false, globalstrict: true */
/*global describe, beforeEach, afterEach, it, expect*/

////////////////////////////////////////////////////////////////////////////////
/// @brief Spec for foxx manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

var FoxxManager = require("org/arangodb/foxx/manager");
var fs = require("fs");
var db = require("internal").db;
var basePath = fs.makeAbsolute(fs.join(module.startupPath(), "common", "test-data", "apps"));
var arango = require("org/arangodb").arango;
var originalEndpoint = arango.getEndpoint().replace(/localhost/, '127.0.0.1');

describe("Foxx Manager", function() {

  describe("using different dbs", function() {

    beforeEach(function() {
      arango.reconnect(originalEndpoint, "_system", "root", "");
      try {
        db._dropDatabase("tmpFMDB");
      }
      catch (err) {
      }
      try {
        db._dropDatabase("tmpFMDB2");
      }
      catch (err) {
      }

      db._createDatabase("tmpFMDB");
      db._createDatabase("tmpFMDB2");
    });

    afterEach(function() {
      arango.reconnect(originalEndpoint, "_system", "root", "");
      db._dropDatabase("tmpFMDB");
      db._dropDatabase("tmpFMDB2");
    });

    it("should allow to install apps on same mountpoint", function() {
      var download = require("internal").download;
      arango.reconnect(originalEndpoint, "tmpFMDB", "root", "");
      try {
        FoxxManager.install(fs.join(basePath, "itzpapalotl"), "/unittest");
      } catch(e) {
        expect(true).toBeFalsy("Could not install a minimal valid app in one DB.");
      }
      arango.reconnect(originalEndpoint, "tmpFMDB2", "root", "");
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), "/unittest");
      } catch(e) {
        expect(true).toBeFalsy("Could not install itzpapalotl in other DB.");
      }
      db._useDatabase("_system");
      var baseUrl = arango.getEndpoint().replace("tcp://", "http://") + "/_db";
      var available = download(baseUrl + "/tmpFMDB/unittest/random");
      expect(available.code).toEqual(200);
      var unavailable = download(baseUrl + "/tmpFMDB2/unittest/random");
      expect(unavailable.code).toEqual(404);
    });

  });

  describe("upgradeing", function() {
    
    var download = require("internal").download;
    var colSetup = "unittest_upgrade_setup";
    var colSetupTeardown = "unittest_upgrade_setup_teardown";
    var mount = "/unittest/upgrade";
    var setupTeardownApp = fs.join(basePath, "minimal-working-setup-teardown");
    var setupApp = fs.join(basePath, "minimal-working-setup");
    var url = arango.getEndpoint().replace("tcp://", "http://") + "/_db/_system" + mount + "/test";
    var brokenApp = fs.join(basePath, "broken-controller-file");

    beforeEach(function() {
      try {
        db._drop(colSetup);
      } catch(e) {
      }
      try {
        db._drop(colSetupTeardown);
      } catch(e) {
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch(e) {
      }
      FoxxManager.install(setupTeardownApp, mount);
      expect(db._collection(colSetupTeardown)).toBeDefined();
      expect(download(url).code).toEqual(200);
    });

    afterEach(function() {
      try {
        db._drop(colSetup);
      } catch(e) {
      }
      try {
        db._drop(colSetupTeardown);
      } catch(e) {
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch(e) {
      }
    });

    it("should run the setup script", function() {
      expect(db._collection(colSetup)).toBeNull();
      FoxxManager.upgrade(setupApp, mount);
      expect(db._collection(colSetup)).toBeDefined();
    });

    it("should not run the teardown script", function() {
      expect(db._collection(colSetupTeardown)).toBeDefined();
      FoxxManager.upgrade(setupApp, mount);
      expect(db._collection(colSetupTeardown)).toBeDefined();
    });

    it("should keep the old app reachable", function() {
      try {
        FoxxManager.upgrade(brokenApp, mount);
      } catch (e) {
      }
      expect(download(url).code).toEqual(200);
    });

    it("should not execute teardown of the old app", function() {
      try {
        FoxxManager.upgrade(brokenApp, mount);
      } catch (e) {
      }
      expect(db._collection(colSetupTeardown)).toBeDefined();
    });

  });

  describe("replacing", function() {
    
    var download = require("internal").download;
    var colSetup = "unittest_replace_setup";
    var colSetupTeardown = "unittest_replace_setup_teardown";
    var mount = "/unittest/replace";
    var setupTeardownApp = fs.join(basePath, "minimal-working-setup-teardown");
    var setupApp = fs.join(basePath, "minimal-working-setup");
    var url = arango.getEndpoint().replace("tcp://", "http://") + "/_db/_system" + mount + "/test";
    var brokenApp = fs.join(basePath, "broken-controller-file");

    beforeEach(function() {
      try {
        db._drop(colSetup);
      } catch(e) {
      }
      try {
        db._drop(colSetupTeardown);
      } catch(e) {
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch(e) {
      }
      FoxxManager.install(setupTeardownApp, mount);
      expect(db._collection(colSetupTeardown)).toBeDefined();
      expect(download(url).code).toEqual(200);
    });

    afterEach(function() {
      try {
        db._drop(colSetup);
      } catch(e) {
      }
      try {
        db._drop(colSetupTeardown);
      } catch(e) {
      }
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch(e) {
      }
    });

    it("should run the setup script", function() {
      expect(db._collection(colSetup)).toBeNull();
      FoxxManager.replace(setupApp, mount);
      expect(db._collection(colSetup)).toBeDefined();
    });

    it("should run the teardown script", function() {
      expect(db._collection(colSetupTeardown)).toBeDefined();
      FoxxManager.replace(setupApp, mount);
      expect(db._collection(colSetupTeardown)).toBeNull();
    });

    it("should make the original app unreachable", function() {
      FoxxManager.replace(setupApp, mount);
      expect(download(url).code).toEqual(404);
    });


    it("with broken app it should keep the old app reachable", function() {
      try {
        FoxxManager.replace(brokenApp, mount);
      } catch (e) {
      }
      expect(download(url).code).toEqual(200);
    });

    it("with broken app it should not execute teardown of the old app", function() {
      try {
        FoxxManager.replace(brokenApp, mount);
      } catch (e) {
      }
      expect(db._collection(colSetupTeardown)).toBeDefined();
    });

  });


});

