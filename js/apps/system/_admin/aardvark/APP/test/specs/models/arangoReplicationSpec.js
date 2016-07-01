/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('Arango Replication Model', function () {
    var myModel = new window.Replication();

    it('verifies defaults', function () {
      expect(myModel.get('state')).toEqual({});
      expect(myModel.get('server')).toEqual({});
    });
  });
}());
