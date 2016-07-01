/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect,
 require, jasmine,  exports,  window, $, arangoLog */
(function () {
  'use strict';

  describe('NotificationCollection', function () {
    var col;

    beforeEach(function () {
      col = new window.NotificationCollection();
    });

    it('NotificationCollection', function () {
      expect(col.model).toEqual(window.Notification);
      expect(col.url).toEqual('');
    });
  });
}());
