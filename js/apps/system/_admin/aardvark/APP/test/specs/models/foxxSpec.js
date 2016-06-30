/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $, jasmine*/

(function () {
  'use strict';

  describe('foxx Model', function () {
    it('is sorted by mount', function () {
      expect(window.Foxx.prototype.idAttribute).toEqual('mount');
    });

    it("can return it's mount url-encoded", function () {
      var testMount = '/this/is_/a/test/mount';
      var expected = encodeURIComponent(testMount);
      var myFoxx = new window.Foxx({
        mount: testMount
      });
      expect(myFoxx.encodedMount()).toEqual(expected);
    });

    it('verifies defaults', function () {
      var myFoxx = new window.Foxx();
      expect(myFoxx.get('name')).toEqual('');
      expect(myFoxx.get('version')).toEqual('Unknown Version');
      expect(myFoxx.get('description')).toEqual('No description');
      expect(myFoxx.get('git')).toEqual('');
      expect(myFoxx.get('isSystem')).toBeFalsy();
      expect(myFoxx.get('development')).toBeFalsy();
    });

    it('is always considered new', function () {
      var myFoxx = new window.Foxx();
      expect(myFoxx.isNew()).toBeFalsy();
    });

    it('system apps should know it', function () {
      var myFoxx = new window.Foxx({
        system: true
      });
      expect(myFoxx.isSystem()).toBeTruthy();
    });

    it('non system apps should know it', function () {
      var myFoxx = new window.Foxx({
        system: false
      });
      expect(myFoxx.isSystem()).toBeFalsy();
    });

    describe('mode chages', function () {
      var myFoxx, expected, call;
      beforeEach(function () {
        var testMount = '/this/is_/a/test/mount';

        myFoxx = new window.Foxx({
          mount: testMount
        });
        expected = {
          value: false
        };
        call = {
          back: function () {
            throw new Error('Should be a spy');
          }
        };
        spyOn($, 'ajax').andCallFake(function (opts) {
          expect(opts.url).toEqual('/_admin/aardvark/foxxes/devel?mount=' + myFoxx.encodedMount());
          expect(opts.type).toEqual('PATCH');
          expect(opts.success).toEqual(jasmine.any(Function));
          expect(opts.error).toEqual(jasmine.any(Function));
          expect(opts.data).toEqual(JSON.stringify(expected.value));
          opts.success();
        });
        spyOn(call, 'back');
      });

      it('should activate production mode', function () {
        myFoxx.set('development', true);
        expected.value = false;
        myFoxx.toggleDevelopment(false, call.back);
        expect($.ajax).toHaveBeenCalled();
        expect(call.back).toHaveBeenCalled();
        expect(myFoxx.get('development')).toEqual(false);
      });

      it('should activate development mode', function () {
        myFoxx.set('development', false);
        expected.value = true;
        myFoxx.toggleDevelopment(true, call.back);
        expect($.ajax).toHaveBeenCalled();
        expect(call.back).toHaveBeenCalled();
        expect(myFoxx.get('development')).toEqual(true);
      });
    });

    describe('configuration', function () {
      var myFoxx;

      beforeEach(function () {
        var testMount = '/this/is_/a/test/mount';
        myFoxx = new window.Foxx({
          mount: testMount
        });
      });

      it('can be read', function () {
        var data = {
          opt1: {
            type: 'string',
            description: 'Option description',
            default: 'empty',
            current: 'empty'
          }
        };
        spyOn($, 'ajax').andCallFake(function (opts) {
          expect(opts.url).toEqual('/_admin/aardvark/foxxes/config?mount=' + myFoxx.encodedMount());
          expect(opts.type).toEqual('GET');
          expect(opts.success).toEqual(jasmine.any(Function));
          expect(opts.error).toEqual(jasmine.any(Function));
          opts.success(data);
        });
        myFoxx.getConfiguration(function (result) {
          expect(result).toEqual(data);
        });
        expect($.ajax).toHaveBeenCalled();
      });

      it('can be saved', function () {
        var data = {
          opt1: 'empty'
        };
        var call = {
          back: function () {
            throw new Error('Should be a spy.');
          }
        };
        spyOn($, 'ajax').andCallFake(function (opts) {
          expect(opts.url).toEqual('/_admin/aardvark/foxxes/config?mount=' + myFoxx.encodedMount());
          expect(opts.type).toEqual('PATCH');
          expect(opts.data).toEqual(JSON.stringify(data));
          expect(opts.success).toEqual(jasmine.any(Function));
          expect(opts.error).toEqual(jasmine.any(Function));
          opts.success(data);
        });
        spyOn(call, 'back');
        myFoxx.setConfiguration(data, call.back);
        expect($.ajax).toHaveBeenCalled();
        expect(call.back).toHaveBeenCalled();
      });
    });

    it('should be able to trigger setup', function () {
      var testMount = '/this/is_/a/test/mount';
      var data = true;
      var myFoxx = new window.Foxx({
        mount: testMount
      });
      spyOn($, 'ajax').andCallFake(function (opts) {
        expect(opts.url).toEqual('/_admin/aardvark/foxxes/setup?mount=' + myFoxx.encodedMount());
        expect(opts.type).toEqual('PATCH');
        expect(opts.success).toEqual(jasmine.any(Function));
        expect(opts.error).toEqual(jasmine.any(Function));
        opts.success(data);
      });
      myFoxx.setup(function (result) {
        expect(result).toEqual(data);
      });
      expect($.ajax).toHaveBeenCalled();
    });

    it('should be able to trigger teardown', function () {
      var testMount = '/this/is_/a/test/mount';
      var data = true;
      var myFoxx = new window.Foxx({
        mount: testMount
      });
      spyOn($, 'ajax').andCallFake(function (opts) {
        expect(opts.url).toEqual('/_admin/aardvark/foxxes/teardown?mount=' + myFoxx.encodedMount());
        expect(opts.type).toEqual('PATCH');
        expect(opts.success).toEqual(jasmine.any(Function));
        expect(opts.error).toEqual(jasmine.any(Function));
        opts.success(data);
      });
      myFoxx.teardown(function (result) {
        expect(result).toEqual(data);
      });
      expect($.ajax).toHaveBeenCalled();
    });

    it('should be able to download', function () {
      var testMount = '/this/is/a/test/mount';
      spyOn(window, 'open');
      var dbName = 'foxx';
      spyOn(window.arango, 'getDatabaseName').andReturn(dbName);
      var myFoxx = new window.Foxx({
        mount: testMount
      });
      myFoxx.download();
      expect(window.open).toHaveBeenCalledWith(
        '/_db/' + dbName + '/_admin/aardvark/foxxes/download/zip?mount=' + myFoxx.encodedMount()
      );
    });
  });
}());
