/* global describe, it, beforeEach, afterEach */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for Foxx manager
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

const FoxxManager = require('@arangodb/foxx/manager');
const arangodb = require('@arangodb');
const ArangoError = arangodb.ArangoError;
const fs = require('fs');
const Module = require('module');
const errors = require('internal').errors;
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
const expect = require('chai').expect;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx Manager install', function () {
  beforeEach(function () {
    try {
      FoxxManager.uninstall('/unittest/broken');
    } catch (e) {
      try {
        // Make sure that the files are physically removed
        let appPath = fs.makeAbsolute(Module._appPath);
        appPath = fs.join(appPath, 'unittest', 'broken');
        fs.removeDirectoryRecursive(appPath, true);
      } catch (e2) {
        // noop
      }
    }
  });

  describe('failing for an invalid app', function () {
    beforeEach(function () {
      try {
        FoxxManager.uninstall('/unittest/broken', {force: true});
      } catch (e) {
        // noop
      }
    });

    afterEach(function () {
      try {
        FoxxManager.uninstall('/unittest/broken');
      } catch (e) {
        // noop
      }
    });

    it('without manifest', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'no-manifest'), '/unittest/broken');
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_SERVICE_MANIFEST_NOT_FOUND.code);
    });

    it('with malformed manifest', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-manifest'), '/unittest/broken');
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_MALFORMED_MANIFEST_FILE.code);
    });

    it('with malformed name', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-name'), '/unittest/broken');
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_SERVICE_MANIFEST.code);
    });

    it('with malformed controller file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-controller-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_SYNTAX_ERROR.code);
        if (require('@arangodb').isServer) {
          expect(err).to.have.property('cause').that.is.an.instanceof(SyntaxError);
        } else {
          expect(err).not.to.have.property('cause');
        }
        return true;
      });
    });

    it('with malformed controller path', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-controller-path'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_NOT_FOUND.code);
        expect(err).not.to.have.property('cause');
        return true;
      });
    });

    it('with broken controller file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'broken-controller-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_SYNTAX_ERROR.code);
        if (require('@arangodb').isServer) {
          expect(err).to.have.property('cause')
            .that.is.an.instanceof(SyntaxError);
        } else {
          expect(err).not.to.have.property('cause');
        }
        return true;
      });
    });

    it('with broken setup file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'broken-setup-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_FAILURE.code);
        if (require('@arangodb').isServer) {
          expect(err).to.have.property('cause');
          expect(err.cause).not.to.be.an.instanceof(SyntaxError);
          expect(err.cause).not.to.be.an.instanceof(ArangoError);
        } else {
          expect(err).not.to.have.property('cause');
        }
        return true;
      });
    });

    it('with malformed exports file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-exports-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_SYNTAX_ERROR.code);
        if (require('@arangodb').isServer) {
          expect(err).to.have.property('cause')
            .that.is.an.instanceof(SyntaxError);
        } else {
          expect(err).not.to.have.property('cause');
        }
        return true;
      });
    });

    it('with malformed exports path', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-exports-path'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_NOT_FOUND.code);
        expect(err).not.to.have.property('cause');
        return true;
      });
    });

    it('with malformed setup file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-setup-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_SYNTAX_ERROR.code);
        if (require('@arangodb').isServer) {
          expect(err).to.have.property('cause')
            .that.is.an.instanceof(SyntaxError);
        } else {
          expect(err).not.to.have.property('cause');
        }
        return true;
      });
    });

    it('with malformed setup path', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'malformed-setup-path'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_NOT_FOUND.code);
        expect(err).not.to.have.property('cause');
        return true;
      });
    });

    it('with missing controller file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'missing-controller-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_NOT_FOUND.code);
        expect(err).not.to.have.property('cause');
        return true;
      });
    });

    it('with missing exports file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'missing-exports-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_NOT_FOUND.code);
        expect(err).not.to.have.property('cause');
        return true;
      });
    });

    it('with missing setup file', function () {
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'missing-setup-file'), '/unittest/broken');
      }).to.throw(ArangoError).that.satisfies(function (err) {
        expect(err).to.have.property('errorNum', errors.ERROR_MODULE_NOT_FOUND.code);
        expect(err).not.to.have.property('cause');
        return true;
      });
    });
  });

  describe('success with', function () {
    it('a minimal app', function () {
      try {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), '/unittest/broken');
      } catch (e) {
        // noop
      }
      FoxxManager.uninstall('/unittest/broken', {force: true});
    });

    it('an app containing a sub-folder "app"', function () {
      try {
        FoxxManager.install(fs.join(basePath, 'interior-app-path'), '/unittest/broken');
      } catch (e) {
        // noop
      }
      FoxxManager.uninstall('/unittest/broken', {force: true});
    });
  });

  describe('should not install on invalid mount point', function () {
    it('starting with _', function () {
      const mount = '/_disallowed';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('starting with %', function () {
      const mount = '/%disallowed';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('starting with a number', function () {
      const mount = '/3disallowed';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('starting with app/', function () {
      const mount = '/app';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('containing /app/', function () {
      const mount = '/unittest/app/test';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('containing a .', function () {
      const mount = '/dis.allowed';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('containing a whitespace', function () {
      const mount = '/disal lowed';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('starting with ?', function () {
      const mount = '/disal?lowed';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });

    it('starting with :', function () {
      const mount = '/disa:llowed';
      expect(function () {
        FoxxManager.install(fs.join(basePath, 'minimal-working-manifest'), mount);
        FoxxManager.uninstall(mount);
      }).to.throw(ArangoError)
        .with.property('errorNum', errors.ERROR_INVALID_MOUNTPOINT.code);
    });
  });
});
