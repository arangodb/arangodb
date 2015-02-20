/*jshint globalstrict: true */
/*global module, require, describe, beforeEach, afterEach, it, expect, arango*/

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
var fs = require("fs");
var db = require("internal").db;
var basePath = fs.makeAbsolute(fs.join(module.startupPath(), "common", "test-data", "apps"));

describe("Foxx Manager", function() {

  describe("using different dbs", function() {

    beforeEach(function() {
      db._useDatabase("_system");
      db._createDatabase("tmpFMDB");
      db._createDatabase("tmpFMDB2");
    });

    afterEach(function() {
      db._useDatabase("_system");
      db._dropDatabase("tmpFMDB");
      db._dropDatabase("tmpFMDB2");
    });

    it("should allow to install apps on same mountpoint", function() {
      var download = require("internal").download;
      db._useDatabase("tmpFMDB");
      try {
        FoxxManager.install(fs.join(basePath, "itzpapalotl"), "/unittest");
      } catch(e) {
        expect(true).toBeFalsy("Could not install a minimal valid app in one DB.");
      }
      db._useDatabase("tmpFMDB2");
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
});

