/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect,
 require, jasmine,  exports, Backbone, window, $, arangoLog */
(function () {
  'use strict';

  describe('ArangoReplication', function () {
    var col;

    beforeEach(function () {
      col = new window.ArangoReplication();
    });

    it('getLogState with success', function () {
      expect(col.url).toEqual('../api/user');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/replication/logger-state');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        opt.success('success');
      });
      expect(col.getLogState()).toEqual('success');
    });
    it('getLogState with error', function () {
      expect(col.url).toEqual('../api/user');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/replication/logger-state');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        opt.error('error');
      });
      expect(col.getLogState()).toEqual('error');
    });

    it('getApplyState with success', function () {
      expect(col.url).toEqual('../api/user');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/replication/applier-state');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        opt.success('success');
      });
      expect(col.getApplyState()).toEqual('success');
    });
    it('getApplyState with error', function () {
      expect(col.url).toEqual('../api/user');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/replication/applier-state');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        opt.error('error');
      });
      expect(col.getApplyState()).toEqual('error');
    });
  });
}());
