/* jshint globalstrict:false, strict:false, globalstrict: true */
/* global describe, it*/

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for foxx manager
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

var FoxxManager = require('@arangodb/foxx/manager');
const expect = require('chai').expect;

require("@arangodb/test-helper").waitForFoxxInitialized();

FoxxManager.update();

var list = FoxxManager.availableJson(false);
expect(list).not.to.be.empty;

describe('Foxx Manager', function () {
  const _it = it;
  for (let service of list) {
    let it = _it;
    describe(`service "${service.name}" from the store`, () => {
      it('should have proper name and author', function () {
        expect(service).to.have.property('name');
        expect(service.name).to.not.be.empty;
        expect(service).to.have.property('latestVersion');
        expect(service.latestVersion).to.not.be.empty;
        expect(service).to.have.property('description');
        expect(service.description).to.not.be.empty;
      });

      if (service.name === 'hello-foxx') {
        // this service cannot be installed at the moment
        it = it.skip;
      }

      it('should be able to be installed', function () {
        var mount = '/unittest/testServices';
        try {
          FoxxManager.uninstall(mount, {force: true});
        } catch (e) {
          // noop
        }
        try {
          FoxxManager.install(service.name, mount, {refresh: false});
        } finally {
          try {
            FoxxManager.uninstall(mount, {force: true});
          } catch (e) {
            // noop
          }
        }
      });
    });
  }
});
