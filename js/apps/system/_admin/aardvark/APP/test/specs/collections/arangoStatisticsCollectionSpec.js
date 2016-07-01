/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, window, afterEach, it, spyOn, expect,
 require, jasmine,  exports,  */
(function () {
  'use strict';

  describe('StatisticsCollection', function () {
    var col;

    beforeEach(function () {
      col = new window.StatisticsCollection();
    });

    it('StatisticsCollection', function () {
      expect(col.model).toEqual(window.Statistics);
      expect(col.url).toEqual('/_admin/statistics');
    });
  });
}());
