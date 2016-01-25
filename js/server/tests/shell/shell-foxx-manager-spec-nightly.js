/*jshint globalstrict:false, strict:false, globalstrict: true */
/*global describe, beforeEach, it*/

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

var FoxxManager = require("@arangodb/foxx/manager");
var expect = require('expect.js');

FoxxManager.update();

var list = FoxxManager.availableJson("match-engines");

describe("Foxx Manager", function() {

  beforeEach(function() {
    FoxxManager.update();
  });

  it("should fetch services from the servicestore", function() {
    expect(list).to.not.be.empty();
  });

  var checkManifest = function(service) {
    it('should have proper name and author', function() {
      expect(service).to.have.property('name');
      expect(service.name).to.not.be.empty();
      expect(service).to.have.property('latestVersion');
      expect(service.latestVersion).to.not.be.empty();
      expect(service).to.have.property('description');
      expect(service.description).to.not.be.empty();
    });
  };

  var i, l = list.length;
  for (i = 0; i < l; ++i) {
    checkManifest(list[i]);
  }

  var testInstall = function(service){
    var mount = "/unittest/testServices";
    try { 
      FoxxManager.uninstall(mount, { force: true });
    } catch(e) {
    }
    if (service.name === "sessions-example-app") {
      return;
      // this service cannot be installed at the moment
      /*
        var arangodb = require("@arangodb");
        var errors = arangodb.errors;
        // Requires oauth2 to be installed
        try {
        FoxxManager.uninstall("/oauth2", { force: true });
        } catch(err) {
        }
        try {
        FoxxManager.install(service.name, mount);
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
        FoxxManager.install(service.name, mount);
        FoxxManager.uninstall(mount);
        } catch(err2) {
        expect(err2).toBeUndefined("Could not install " + service.name);
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
        FoxxManager.install(service.name, mount);
        FoxxManager.uninstall(mount);
      } catch(e) {
        expect(e).to.not.be.undefined()
        ("Could not install " + service.name + " - \n" + e.stack);
        try {
          FoxxManager.uninstall(mount, { force: true });
        } catch(err) {
        }
      }
    }
  };

  list.forEach(function(service) {
    it("should be able to install '" + service.name + "' from store", function() {
      testInstall(service);
    });
  });

});

