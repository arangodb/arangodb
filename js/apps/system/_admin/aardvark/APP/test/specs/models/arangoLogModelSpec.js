/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('Arango Log Model', function () {
    var model;

    beforeEach(function () {
      model = new window.newArangoLog();
    });

    it('verifies defaults', function () {
      expect(model.get('lid')).toEqual('');
      expect(model.get('level')).toEqual('');
      expect(model.get('timestamp')).toEqual('');
      expect(model.get('text')).toEqual('');
      expect(model.get('totalAmount')).toEqual('');
    });

    describe('parses the status', function () {
      it('Error', function () {
        model.set('level', 1);
        expect(model.getLogStatus()).toEqual('Error');
      });

      it('Warning', function () {
        model.set('level', 2);
        expect(model.getLogStatus()).toEqual('Warning');
      });

      it('Info', function () {
        model.set('level', 3);
        expect(model.getLogStatus()).toEqual('Info');
      });

      it('Debug', function () {
        model.set('level', 4);
        expect(model.getLogStatus()).toEqual('Debug');
      });

      it('Unknown default', function () {
        model.set('level', 'wrong value');
        expect(model.getLogStatus()).toEqual('Unknown');
      });
    });
  });
}());
