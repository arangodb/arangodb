/*jshint globalstrict:false, strict:false, globalstrict: true */
/*global describe, beforeEach, it, expect*/

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

describe("Foxx Manager", function() {

  beforeEach(function() {
    FoxxManager.update();
  });

  it("should fetch apps from the appstore", function() {
    var list = FoxxManager.availableJson();
    var i, app, l = list.length;
    expect(l).toBeGreaterThan(0);
    for (i = 0; i < l; ++i) {
      app = list[i];
      expect(app.name).toBeDefined("App has no name");
      expect(app.latestVersion).toBeDefined("App " + app.name + " has no latestVersion.");
      expect(app.author).toBeDefined("App " + app.name + " has no author.");
      expect(app.description).toBeDefined("App " + app.name + " has no description");
    }
  });

  it("should be able to install all apps from appstore", function() {
    var mount = "/unittest/testApps";
    try { 
      FoxxManager.uninstall(mount, { force: true });
    } catch(e) {
    }
    var list = FoxxManager.availableJson();
    var i, app, l = list.length;
    for (i = 0; i < l; ++i) {
      app = list[i];
      if (app.name === "sessions-example-app") {
        // this app cannot be installed at the moment
        continue;
        /*
        var arangodb = require("org/arangodb");
        var errors = arangodb.errors;
        // Requires oauth2 to be installed
        try {
          FoxxManager.uninstall("/oauth2", { force: true });
        } catch(err) {
        }
        try {
          FoxxManager.install(app.name, mount);
          expect(true).toBeFalsy("Installing sessions-example-app should require oauth2");
        } catch(e) {
          expect(e.errorNum).toEqual(errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code);
          try {
            FoxxManager.install("oauth2", "/oauth2");
          } catch(err) {
            expect(e).toBeUndefined("Could not install oauth2");
            continue;
          }
          try {
            FoxxManager.install(app.name, mount);
            FoxxManager.uninstall(mount);
          } catch(err2) {
            expect(err2).toBeUndefined("Could not install " + app.name);
            try {
              FoxxManager.uninstall(mount, { force: true });
            } catch(err) {
            }
          }
          try {
            FoxxManager.uninstall("/oauth2", {force: true});
          } catch(err) {
          }
        }
        */
      } else {
        try {
          FoxxManager.install(app.name, mount);
          FoxxManager.uninstall(mount);
        } catch(e) {
          expect(e).toBeUndefined("Could not install " + app.name + " - \n" + e.stack);
          try {
            FoxxManager.uninstall(mount, { force: true });
          } catch(err) {
          }
        }
      }
    }
  });

});

