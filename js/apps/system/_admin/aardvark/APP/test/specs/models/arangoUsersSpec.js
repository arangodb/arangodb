/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/* global $*/

(function () {
  'use strict';

  describe('Arango Users Model', function () {
    var user = 'myUser',
      myModel = new window.Users({user: user});

    it('verifies url', function () {
      expect(myModel.url()).toEqual('/_api/user');
    });

    it('verifies defaults', function () {
      expect(myModel.get('active')).toBeFalsy();
      expect(myModel.get('extra')).toEqual({});
    });

    it('verifies setExtras', function () {
      var name = 'name',
        img = 'myImg';
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/user/' + user);
        expect(opt.type).toEqual('PATCH');
      });
      myModel.setExtras(name, img);
    });

    it('verifies setPassword', function () {
      var passwd = 'passwd';
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/user/' + user);
        expect(opt.type).toEqual('PATCH');
      });
      myModel.setPassword(passwd);
    });

    it('verifies checkPassword', function () {
      var passwd = 'passwd',
        goodResult = 'goodResult',
        result = 'badResult';
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/user/' + user);
        expect(opt.type).toEqual('POST');
        expect(opt.success).toEqual(jasmine.any(Function));
        opt.success({
          result: goodResult
        });
      });
      result = myModel.checkPassword(passwd);
      expect(result).toEqual(goodResult);
    });

    it('verifies parse/isNew/url', function () {
      expect(myModel.parse(true)).toBeTruthy();
      expect(myModel.isNew()).toBeFalsy();
      expect(myModel.url()).toEqual('/_api/user/' + user);
    });
  });

  describe('Arango Users Model empty', function () {
    var myModel = new window.Users();
    it('verifies url', function () {
      expect(myModel.parse(true)).toBeTruthy();
      expect(myModel.isNew()).toBeFalsy();
      expect(myModel.url()).toEqual('/_api/user');
    });
  });
}());
