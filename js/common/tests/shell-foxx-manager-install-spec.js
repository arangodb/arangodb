/*jshint globalstrict: true */
/*global module, require, describe, beforeEach, afterEach, it, expect*/

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

"use strict";

var FoxxManager = require("org/arangodb/foxx/manager");
var arangodb = require("org/arangodb");
var ArangoError = arangodb.ArangoError;
var fs = require("fs");
var errors = require("internal").errors;
var basePath = fs.makeAbsolute(fs.join(module.startupPath(), "common", "test-data", "apps"));

describe("Foxx Manager install", function() {

  beforeEach(function() {
    try {
      FoxxManager.uninstall("/unittest/broken");
    } catch(e) {
      try {
        // Make sure that the files are physically removed
        var appPath = fs.makeAbsolute(module.appPath());
        appPath = fs.join(appPath, "unittest", "broken");
        fs.removeDirectoryRecursive(appPath, true);
      } catch(err) {
      }
    }
  });

  describe("failing for an invalid app", function() {
    beforeEach(function() {
      try {
        FoxxManager.uninstall("/unittest/broken", {force: true});
      } catch(e) {
      }
    });

    afterEach(function() {
      try {
        FoxxManager.uninstall("/unittest/broken");
      } catch(e) {
      }
    });

    it("without manifest", function() {
      try {
        FoxxManager.install(fs.join(basePath, "no-manifest"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FILE_NOT_FOUND.code);
      }
    });

    it("with malformed manifest", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-manifest"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_APPLICATION_MANIFEST.code);
      }
    });

    it("with incomplete manifest", function() {
      try {
        FoxxManager.install(fs.join(basePath, "incomplete-manifest"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_MANIFEST_FILE_ATTRIBUTE_MISSING.code);
      }
    });

    it("with malformed name", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-name"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_APPLICATION_MANIFEST.code);
      }
    });

    it("with malformed version", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-version"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_APPLICATION_MANIFEST.code);
      }
    });

    it("with malformed controller file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-controller-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_SYNTAX_ERROR_IN_SCRIPT.code);
      }
    });

    it("with malformed controller path", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-controller-name"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("with malformed controller path", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-controller-path"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FILE_NOT_FOUND.code);
      }
    });

    it("with broken controller file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "broken-controller-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code);
      }
    });

    it("with broken exports file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "broken-exports-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code);
      }
    });

    it("with broken setup file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "broken-setup-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code);
      }
    });

    it("with malformed exports file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-exports-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_SYNTAX_ERROR_IN_SCRIPT.code);
      }
    });

    it("with malformed exports path", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-exports-path"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FILE_NOT_FOUND.code);
      }
    });

    it("with malformed setup file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-setup-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_SYNTAX_ERROR_IN_SCRIPT.code);
      }
    });

    it("with malformed setup path", function() {
      try {
        FoxxManager.install(fs.join(basePath, "malformed-setup-path"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FILE_NOT_FOUND.code);
      }
    });

    it("with missing controller file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "missing-controller-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FILE_NOT_FOUND.code);
      }
    });

    it("with missing exports file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "missing-exports-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FILE_NOT_FOUND.code);
      }
    });

    it("with missing setup file", function() {
      try {
        FoxxManager.install(fs.join(basePath, "missing-setup-file"), "/unittest/broken");
        expect(true).toBeFalsy("Managed to install broken application");
      } catch(e) {
        expect(e instanceof ArangoError).toBeTruthy();
        expect(e.errorNum).toEqual(errors.ERROR_FILE_NOT_FOUND.code);
      }
    });
  });

  describe("success with", function() {

    it("a minimal app", function() {
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), "/unittest/broken");
      } catch(e) {
        expect(true).toBeFalsy("Could not install a minimal valid app.");
      }
      FoxxManager.uninstall("/unittest/broken", {force: true});
    });

    it("an app containing a sub-folder 'app'", function() {
      try {
        FoxxManager.install(fs.join(basePath, "interior-app-path"), "/unittest/broken");
      } catch(e) {
        expect(true).toBeFalsy("Could not install an app with sub-folder 'app'.");
      }
      FoxxManager.uninstall("/unittest/broken", {force: true});
    });

  });

  describe("should not install on invalid mountpoint", function() {

    it("starting with _", function() {
      var mount = "/_disallowed";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("starting with %", function() {
      var mount = "/%disallowed";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("starting with a number", function() {
      var mount = "/3disallowed";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("starting with app/", function() {
      var mount = "/app";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("containing /app/", function() {
      var mount = "/unittest/app/test";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("containing a .", function() {
      var mount = "/dis.allowed";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("containing a whitespace", function() {
      var mount = "/disal lowed";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("starting with ?", function() {
      var mount = "/disal?lowed";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

    it("starting with :", function() {
      var mount = "/disa:llowed";
      try {
        FoxxManager.install(fs.join(basePath, "minimal-working-manifest"), mount);
        expect(true).toBeFalsy("Installed app at invalid mountpoint.");
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e.errorNum).toEqual(errors.ERROR_INVALID_MOUNTPOINT.code);
      }
    });

  });

});
