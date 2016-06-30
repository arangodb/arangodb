/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('Arango Statistics Model', function () {
    it('verifies url', function () {
      var myStatistics = new window.Statistics();
      expect(myStatistics.url())
        .toEqual('/_admin/statistics');
    });
  });
}());
