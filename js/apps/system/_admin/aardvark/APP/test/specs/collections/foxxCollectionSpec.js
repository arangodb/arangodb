/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, it, expect, exports, Backbone, window, $, arangoLog */
/* global runs, waitsFor, spyOn, jasmine */
(function () {
  'use strict';

  var validateRequest = function (url, type, data) {
    var lastCall = $.ajax.mostRecentCall.args[0];
    expect(lastCall.url).toEqual(url, 'URL');
    expect(lastCall.type).toEqual(type, 'type');
    expect(lastCall.data).toEqual(data, 'data');
    expect(lastCall.success).toEqual(jasmine.any(Function), 'Success Handler');
    expect(lastCall.error).toEqual(jasmine.any(Function), 'Error Handler');
  };

  describe('FoxxCollection', function () {
    var col;

    beforeEach(function () {
      col = new window.FoxxCollection();
    });

    it('should use Foxx as its model', function () {
      expect(col.model).toEqual(window.Foxx);
    });

    it('should point to foxxes base path', function () {
      expect(col.url).toEqual('/_admin/aardvark/foxxes');
    });

    describe('sorting apps', function () {
      var first, second, third;
      beforeEach(function () {
        first = '/_test';
        second = '/aal';
        third = '/zztop';
        col.add({mount: second});
        col.add({mount: third});
        col.add({mount: first});
      });

      it('should always be by mount', function () {
        col.sort();
        expect(col.models[0].get('mount')).toEqual(first);
        expect(col.models[1].get('mount')).toEqual(second);
        expect(col.models[2].get('mount')).toEqual(third);
      });

      it('should be by mount and reversable', function () {
        col.setSortingDesc(true);
        col.sort();
        expect(col.models[2].get('mount')).toEqual(first);
        expect(col.models[1].get('mount')).toEqual(second);
        expect(col.models[0].get('mount')).toEqual(third);
      });
    });

    describe('installing apps', function () {
      it('should generate an app', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            name: 'My App',
            author: 'ArangoDB',
            description: 'Description of the App',
            license: 'Apache 2',
            collectionNames: ['first', 'second']
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.generate(info, mount, function () {
            calledBack = true;
          });
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/generate?mount=' + encodeURIComponent(mount);
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from github', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            url: 'arangodb/itzpapalotl',
            version: '1.2.0'
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromGithub(info, mount, function () {
            calledBack = true;
          });
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/git?mount=' + encodeURIComponent(mount);
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from store', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            name: 'itzpapalotl',
            version: '1.2.0'
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromStore(info, mount, function () {
            calledBack = true;
          });
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/store?mount=' + encodeURIComponent(mount);
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from an uploaded zip', function () {
        var calledBack, info, mount;

        runs(function () {
          var fileName = 'uploads/tmp-123.zip';
          calledBack = false;
          info = {
            zipFile: fileName
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromZip(fileName, mount, function () {
            calledBack = true;
          });
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/zip?mount=' + encodeURIComponent(mount);
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });
    });

    describe('upgrading apps', function () {
      it('should generate an app', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            name: 'My App',
            author: 'ArangoDB',
            description: 'Description of the App',
            license: 'Apache 2',
            collectionNames: ['first', 'second']
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.generate(info, mount, function () {
            calledBack = true;
          }, false);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/generate?mount=' + encodeURIComponent(mount) + '&upgrade=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from github', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            url: 'arangodb/itzpapalotl',
            version: '1.2.0'
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromGithub(info, mount, function () {
            calledBack = true;
          }, false);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/git?mount=' + encodeURIComponent(mount) + '&upgrade=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from store', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            name: 'itzpapalotl',
            version: '1.2.0'
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromStore(info, mount, function () {
            calledBack = true;
          }, false);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/store?mount=' + encodeURIComponent(mount) + '&upgrade=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from an uploaded zip', function () {
        var calledBack, info, mount;

        runs(function () {
          var fileName = 'uploads/tmp-123.zip';
          calledBack = false;
          info = {
            zipFile: fileName
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromZip(fileName, mount, function () {
            calledBack = true;
          }, false);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/zip?mount=' + encodeURIComponent(mount) + '&upgrade=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });
    });

    describe('replacing apps', function () {
      it('should generate an app', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            name: 'My App',
            author: 'ArangoDB',
            description: 'Description of the App',
            license: 'Apache 2',
            collectionNames: ['first', 'second']
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.generate(info, mount, function () {
            calledBack = true;
          }, true);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/generate?mount=' + encodeURIComponent(mount) + '&replace=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from github', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            url: 'arangodb/itzpapalotl',
            version: '1.2.0'
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromGithub(info, mount, function () {
            calledBack = true;
          }, true);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/git?mount=' + encodeURIComponent(mount) + '&replace=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from store', function () {
        var calledBack, info, mount;

        runs(function () {
          calledBack = false;
          info = {
            name: 'itzpapalotl',
            version: '1.2.0'
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromStore(info, mount, function () {
            calledBack = true;
          }, true);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/store?mount=' + encodeURIComponent(mount) + '&replace=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });

      it('should install an app from an uploaded zip', function () {
        var calledBack, info, mount;

        runs(function () {
          var fileName = 'uploads/tmp-123.zip';
          calledBack = false;
          info = {
            zipFile: fileName
          };
          mount = '/my/app';
          spyOn($, 'ajax').andCallFake(function (opts) {
            opts.success();
          });
          col.installFromZip(fileName, mount, function () {
            calledBack = true;
          }, true);
        });

        waitsFor(function () {
          return calledBack;
        }, 750);

        runs(function () {
          var url = '/_admin/aardvark/foxxes/zip?mount=' + encodeURIComponent(mount) + '&replace=true';
          validateRequest(url, 'PUT', JSON.stringify(info));
        });
      });
    });
  });
}());
