/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, arangoHelper, afterEach, it, spyOn, expect*/
/* global $, jasmine, _, window, document*/

(function () {
  'use strict';

  describe('version check', function () {
    it('should request version from server', function () {
      spyOn($, 'ajax');
      window.checkVersion();
      expect($.ajax).toHaveBeenCalledWith({
        type: 'GET',
        cache: false,
        url: '/_api/version',
        contentType: 'application/json',
        processData: false,
        async: true,
        success: jasmine.any(Function)
      });
    });

    describe('showing the dialog', function () {
      var fakeRequest, current, modalDiv, data;

      beforeEach(function () {
        modalDiv = document.createElement('div');
        modalDiv.id = 'modalPlaceholder';
        document.body.appendChild(modalDiv);
        current = '2.0.2';
        var successCallback;
        data = {
          version: current
        };
        fakeRequest = function (opts) {
          successCallback = opts.success;
        };
        spyOn($, 'ajax').andCallFake(function (opts) {
          fakeRequest(opts);
          fakeRequest = function () {
            return;
          };
        });
        window.checkVersion();
        expect(window.parseVersions).toBeUndefined();
        spyOn(window.versionHelper, 'fromString').andCallThrough();
        $.ajax.reset();
        successCallback(data);
        expect(window.versionHelper.fromString).toHaveBeenCalledWith(data.version);
        expect(window.parseVersions).toEqual(jasmine.any(Function));
        expect($.ajax).toHaveBeenCalledWith({
          type: 'GET',
          async: true,
          crossDomain: true,
          dataType: 'jsonp',
          url: 'https://www.arangodb.com/repositories/versions.php' +
            '?jsonp=parseVersions&version=' + encodeURIComponent(current)
        });
        $.ajax.reset();
        window.modalView = new window.ModalView();
        spyOn(window.modalView, 'show').andCallThrough();
      });

      afterEach(function () {
        delete window.parseVersions;
        delete window.modalView;
        document.body.removeChild(modalDiv);
      });

      it('should not show a dialog if no new version is available', function () {
        window.parseVersions({});
        expect(window.modalView.show).not.toHaveBeenCalled();
      });

      it('should not show a dialog for devel version', function () {
        current = '1.0.4-devel';
        data.version = current;
        window.parseVersions({
          major: '3.0.1'
        });
        expect(window.modalView.show).not.toHaveBeenCalled();
      });

      it('should not show a dialog if user disabled it', function () {
        fakeRequest = function (opts) {
          opts.success(false);
        };
        window.parseVersions({
          major: '3.0.1'
        });
        expect(window.modalView.show).not.toHaveBeenCalled();
        expect($.ajax).toHaveBeenCalledWith({
          type: 'GET',
          url: '/_admin/aardvark/shouldCheckVersion',
          success: jasmine.any(Function)
        });
      });

      describe('if dialog is allowed', function () {
        beforeEach(function () {
          fakeRequest = function (opts) {
            opts.success(true);
          };
          spyOn(window.modalView, 'createReadOnlyEntry').andCallThrough();
        });

        describe('buttons', function () {
          it('should create a download button', function () {
            window.parseVersions({
              major: {
                version: '3.0.1'
              }
            });
            spyOn(window, 'open');
            spyOn(window.modalView, 'hide');
            $('#modalButton2').click();
            expect(window.open).toHaveBeenCalledWith(
              'https://www.arangodb.com/download',
              '_blank'
            );
            expect(window.modalView.hide).toHaveBeenCalled();
          });

          it('should create the never remind button', function () {
            window.parseVersions({
              major: {
                version: '3.0.1'
              }
            });
            fakeRequest = function () {
              return;
            };
            $.ajax.reset();
            spyOn(window.modalView, 'hide');
            $('#modalButton0').click();
            expect($.ajax).toHaveBeenCalledWith({
              type: 'POST',
              url: '/_admin/aardvark/disableVersionCheck'
            });
            expect(window.modalView.hide).toHaveBeenCalled();
          });
        });

        it('should offer a new major and bugfix', function () {
          window.parseVersions({
            major: {
              version: '3.0.1'
            },
            bugfix: {
              version: '2.0.4'
            }
          });
          expect(window.modalView.createReadOnlyEntry)
            .toHaveBeenCalledWith('current', 'Current', current);
          expect(window.modalView.createReadOnlyEntry)
            .toHaveBeenCalledWith('major', 'Major', '3.0.1');
          expect(window.modalView.createReadOnlyEntry)
            .toHaveBeenCalledWith('bugfix', 'Bugfix', '2.0.4');
          expect(window.modalView.createReadOnlyEntry).not
            .toHaveBeenCalledWith('minor', 'Minor', jasmine.any(String));
        });

        it('should offer a new minor', function () {
          window.parseVersions({
            minor: {
              version: '2.1.1'
            }
          });
          expect(window.modalView.createReadOnlyEntry)
            .toHaveBeenCalledWith('current', 'Current', current);
          expect(window.modalView.createReadOnlyEntry).not
            .toHaveBeenCalledWith('major', 'Major', jasmine.any(String));
          expect(window.modalView.createReadOnlyEntry).not
            .toHaveBeenCalledWith('bugfix', 'Bugfix', jasmine.any(String));
          expect(window.modalView.createReadOnlyEntry)
            .toHaveBeenCalledWith('minor', 'Minor', '2.1.1');
        });
      });
    });
  });
}());
