/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('The current database', function () {
    var model;

    beforeEach(function () {
      model = new window.CurrentDatabase();
    });

    it('should request /_api/database/current on fetch', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/database/current');
        expect(opt.type).toEqual('GET');
      });
      model.fetch();
    });

    it('should request /_api/database/current on fetch', function () {
      expect(model.parse({result: 'a'})).toEqual('a');
    });
  });
}());
