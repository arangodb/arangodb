/* jshint browser: true */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global runs, waitsFor, jasmine*/
/* global $*/

(function () {
  'use strict';

  describe('Foxx Install View', function () {
    var view, listDummy, closeButton, generateButton, modalDiv,
      uploadCallback, storeApp, storeAppVersion;

    beforeEach(function () {
      modalDiv = document.createElement('div');
      modalDiv.id = 'modalPlaceholder';
      document.body.appendChild(modalDiv);
      window.modalView = new window.ModalView();
      closeButton = '#modalButton0';
      generateButton = '#modalButton1';
      window.CreateDummyForObject(window, 'FoxxCollection');
      listDummy = new window.FoxxCollection();
      view = new window.FoxxInstallView({
        collection: listDummy
      });
      storeApp = 'Online App';
      storeAppVersion = '2.1.1';
      spyOn(listDummy, 'fetch').andCallFake(function (opts) {
        if (opts && opts.success) {
          opts.success();
        }
      });
      spyOn(listDummy, 'each');
      spyOn(listDummy, 'sort');
      spyOn($.fn, 'uploadFile').andCallFake(function (opts) {
        expect(opts.url).toEqual('/_api/upload?multipart=true');
        expect(opts.allowedTypes).toEqual('zip');
        expect(opts.onSuccess).toEqual(jasmine.any(Function));
        uploadCallback = opts.onSuccess;
      });
      spyOn($, 'get').andCallFake(function (url, callback) {
        expect(url).toEqual('foxxes/fishbowl');
        callback([
          {
            name: 'MyApp',
            author: 'ArangoDB',
            description: 'Description of the app',
            latestVersion: '1.1.0'
          }, {
            name: storeApp,
            author: 'ArangoDB',
            description: 'Description of the other app',
            latestVersion: storeAppVersion
          }
        ]);
        return {
          fail: function () {}
        };
      });
    });

    afterEach(function () {
      delete window.modalView;
      document.body.removeChild(modalDiv);
    });

    describe('Adding a new App', function () {
      var spyFunc;

      beforeEach(function () {
        runs(function () {
          spyFunc = jasmine.createSpy('reload');
          view.install(spyFunc);
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'block';
        }, 'The Add App dialog should be shown', 750);
      });

      it('should open and close the install dialog', function () {
        runs(function () {
          $(closeButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);
      });

      it('should offer the mountpoint input but not the run teardown', function () {
        runs(function () {
          expect($('#new-app-mount').length).toEqual(1);
          expect($('#new-app-teardown').length).toEqual(0);
          $(closeButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);
      });

      it('should generate an empty app', function () {
        var mount, author, name, desc, license, expectedInfo;

        runs(function () {
          $('#tab-new').click();
          spyOn(listDummy, 'generate').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          mount = '/my/application';
          author = 'ArangoDB';
          name = 'MyApp';
          desc = 'My new app';
          license = 'Apache 2';
          expectedInfo = {
            author: author,
            name: name,
            description: desc,
            license: license,
            collectionNames: []
          };

          expect($(generateButton).prop('disabled')).toBeTruthy();

          $('#new-app-mount').val(mount);
          $('#new-app-author').val(author);
          $('#new-app-name').val(name);
          $('#new-app-description').val(desc);
          $('#new-app-license').val(license);
          $('#new-app-mount').keyup();
          expect($(generateButton).prop('disabled')).toBeFalsy();
          $(generateButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.generate).toHaveBeenCalledWith(
            expectedInfo, mount, jasmine.any(Function)
          );
        });
      });

      it('should install an app from the store', function () {
        var mount;

        runs(function () {
          var button;
          mount = '/my/application';
          $('#tab-store').click();
          $('#new-app-mount').val(mount);
          $('#new-app-mount').keyup();
          spyOn(listDummy, 'installFromStore').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          expect($('#appstore-content').children().length).toEqual(2);
          button = $("#appstore-content .install-app[appId='" + storeApp + "']");
          expect(button.length).toEqual(1);
          button.click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromStore).toHaveBeenCalledWith(
            {name: storeApp, version: storeAppVersion}, mount, jasmine.any(Function)
          );
        });
      });

      it('should install an app from github', function () {
        var repository, version, mount, expectedInfo;
        runs(function () {
          $('#tab-git').click();
          spyOn(listDummy, 'installFromGithub').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          repository = 'arangodb/itzpapalotl';
          version = '1.2.0';
          mount = '/my/application';
          expectedInfo = {
            url: repository,
            version: version
          };
          $('#new-app-mount').val(mount);
          $('#repository').val(repository);
          $('#tag').val(version);
          $('#new-app-mount').keyup();
          expect($(generateButton).prop('disabled')).toBeFalsy();
          $(generateButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromGithub).toHaveBeenCalledWith(
            expectedInfo, mount, jasmine.any(Function)
          );
        });
      });

      it('should install an app from zip file', function () {
        var fileName = '/tmp/upload-12345.zip';
        var mount = '/my/application';

        runs(function () {
          $('#tab-zip').click();
          spyOn(listDummy, 'installFromZip').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          $('#new-app-mount').val(mount);
          uploadCallback(['app.zip'], {filename: fileName});
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromZip).toHaveBeenCalledWith(
            fileName, mount, jasmine.any(Function)
          );
        });
      });
    });

    describe('Upgradeing App', function () {
      var spyFunc, mount;

      beforeEach(function () {
        runs(function () {
          mount = '/my/application';
          spyFunc = jasmine.createSpy('reload');
          view.upgrade(mount, spyFunc);
          $('#new-app-teardown').prop('checked', false);
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'block';
        }, 'The Add App dialog should be shown', 750);
      });

      it('should open and close the dialog', function () {
        runs(function () {
          $(closeButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);
      });

      it('should not offer the mountpoint input but the run teardown', function () {
        runs(function () {
          expect($('#new-app-mount').length).toEqual(0);
          expect($('#new-app-teardown').length).toEqual(1);
          $(closeButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);
      });

      it('generate an empty app', function () {
        var author, name, desc, license, expectedInfo;

        runs(function () {
          $('#tab-new').click();
          spyOn(listDummy, 'generate').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          author = 'ArangoDB';
          name = 'MyApp';
          desc = 'My new app';
          license = 'Apache 2';
          expectedInfo = {
            author: author,
            name: name,
            description: desc,
            license: license,
            collectionNames: []
          };

          expect($(generateButton).prop('disabled')).toBeTruthy();

          $('#new-app-author').val(author);
          $('#new-app-name').val(name);
          $('#new-app-description').val(desc);
          $('#new-app-license').val(license);
          $('#new-app-license').keyup();
          expect($(generateButton).prop('disabled')).toBeFalsy();
          $(generateButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.generate).toHaveBeenCalledWith(
            expectedInfo, mount, jasmine.any(Function), false
          );
        });
      });

      it('an app from the store', function () {
        runs(function () {
          var button;
          $('#tab-store').click();
          spyOn(listDummy, 'installFromStore').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          expect($('#appstore-content').children().length).toEqual(2);
          button = $("#appstore-content .install-app[appId='" + storeApp + "']");
          expect(button.length).toEqual(1);
          button.click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromStore).toHaveBeenCalledWith(
            {name: storeApp, version: storeAppVersion}, mount, jasmine.any(Function), false
          );
        });
      });

      it('an app from github', function () {
        var repository, version, expectedInfo;

        runs(function () {
          $('#tab-git').click();
          spyOn(listDummy, 'installFromGithub').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          repository = 'arangodb/itzpapalotl';
          version = '1.2.0';
          expectedInfo = {
            url: repository,
            version: version
          };
          $('#repository').val(repository);
          $('#tag').val(version).keyup();
          expect($(generateButton).prop('disabled')).toBeFalsy();
          $(generateButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromGithub).toHaveBeenCalledWith(
            expectedInfo, mount, jasmine.any(Function), false
          );
        });
      });

      it('an app from zip file', function () {
        var fileName = '/tmp/upload-12345.zip';

        runs(function () {
          $('#tab-zip').click();
          spyOn(listDummy, 'installFromZip').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          uploadCallback(['app.zip'], {filename: fileName});
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromZip).toHaveBeenCalledWith(
            fileName, mount, jasmine.any(Function), false
          );
        });
      });
    });

    describe('Replacing App', function () {
      var spyFunc, mount;

      beforeEach(function () {
        runs(function () {
          mount = '/my/application';
          spyFunc = jasmine.createSpy('reload');
          view.upgrade(mount, spyFunc);
          $('#new-app-teardown').prop('checked', true);
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'block';
        }, 'The Add App dialog should be shown', 750);
      });

      it('should open and close the dialog', function () {
        runs(function () {
          $(closeButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);
      });

      it('should not offer the mountpoint input but the run teardown', function () {
        runs(function () {
          expect($('#new-app-mount').length).toEqual(0);
          expect($('#new-app-teardown').length).toEqual(1);
          $(closeButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);
      });

      it('generate an empty app', function () {
        var author, name, desc, license, expectedInfo;

        runs(function () {
          $('#tab-new').click();
          spyOn(listDummy, 'generate').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          author = 'ArangoDB';
          name = 'MyApp';
          desc = 'My new app';
          license = 'Apache 2';
          expectedInfo = {
            author: author,
            name: name,
            description: desc,
            license: license,
            collectionNames: []
          };

          expect($(generateButton).prop('disabled')).toBeTruthy();

          $('#new-app-author').val(author);
          $('#new-app-name').val(name);
          $('#new-app-description').val(desc);
          $('#new-app-license').val(license);
          $('#new-app-license').keyup();
          expect($(generateButton).prop('disabled')).toBeFalsy();
          $(generateButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.generate).toHaveBeenCalledWith(
            expectedInfo, mount, jasmine.any(Function), true
          );
        });
      });

      it('an app from the store', function () {
        runs(function () {
          var button;
          $('#tab-store').click();
          spyOn(listDummy, 'installFromStore').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          expect($('#appstore-content').children().length).toEqual(2);
          button = $("#appstore-content .install-app[appId='" + storeApp + "']");
          expect(button.length).toEqual(1);
          button.click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromStore).toHaveBeenCalledWith(
            {name: storeApp, version: storeAppVersion}, mount, jasmine.any(Function), true
          );
        });
      });

      it('an app from github', function () {
        var repository, version, expectedInfo;

        runs(function () {
          $('#tab-git').click();
          spyOn(listDummy, 'installFromGithub').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          repository = 'arangodb/itzpapalotl';
          version = '1.2.0';
          expectedInfo = {
            url: repository,
            version: version
          };
          $('#repository').val(repository);
          $('#tag').val(version).keyup();
          expect($(generateButton).prop('disabled')).toBeFalsy();
          $(generateButton).click();
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromGithub).toHaveBeenCalledWith(
            expectedInfo, mount, jasmine.any(Function), true
          );
        });
      });

      it('an app from zip file', function () {
        var fileName = '/tmp/upload-12345.zip';

        runs(function () {
          $('#tab-zip').click();
          spyOn(listDummy, 'installFromZip').andCallFake(function (i, m, callback) {
            callback({
              error: false
            });
          });
          uploadCallback(['app.zip'], {filename: fileName});
        });

        waitsFor(function () {
          return $('#modal-dialog').css('display') === 'none';
        }, 'The Add App dialog should be hidden.', 750);

        runs(function () {
          expect(spyFunc).toHaveBeenCalled();
          expect(listDummy.installFromZip).toHaveBeenCalledWith(
            fileName, mount, jasmine.any(Function), true
          );
        });
      });
    });
  });
}());
