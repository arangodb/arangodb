/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jQuery, _, jqconsole, $*/
/* global arangoHelper*/

(function () {
  'use strict';

  describe('The shell view', function () {
    var view, div, jQueryDummy;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);

      view = new window.shellView({
      });
      spyOn(view, 'resize');

      view.render();
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    it('assert the basics', function () {
      expect(view.resizing).toEqual(false);
    });

    it('should render a js command', function () {});

    it('should execute a js command', function () {
      view.executeJs('1336+1');
      var toExpect = jQuery('.jssuccess').last().children().text();
      expect(toExpect).toContain('1337');
      expect(toExpect).toContain('==>');
    });

    it('should use an undefined variable', function () {
      view.executeJs('hallo');
      var toExpect = jQuery('.jserror').last().children().text();
      expect(toExpect).toContain('ReferenceError');
      expect(toExpect).toContain(toExpect);
    });

    it('should execute a false js command', function () {
      view.executeJs('1asdfkoasd234g3,.o!afdsg');
      var toExpect = jQuery('.jserror').last().children().text();
      expect(toExpect).toContain('SyntaxError');
      expect(toExpect).toContain('Parse error');
      expect(toExpect).toContain(toExpect);
    });

    it('should execute a special js command: help', function () {
      view.executeJs('help');
      var toExpect = jQuery('.jssuccess').last().children().text();
      expect(toExpect).toContain('help');
    });

    it('should execute an incomplete command', function () {
      view.executeJs('2 + ');
      var toExpect = jQuery('.jssuccess').last().children().text();
      expect(toExpect).toContain('...');
    });

    it('should check js shell hotkeys functionality', function () {
      var counter = 0;
      _.each(jqconsole.shortcuts, function (k, v) {
        counter++;
        _.each(k, function (x, y) {
          x();
        });
      });
      expect(counter).toBe(4);
    });

    it('should execute a command using enter-keypress', function () {
      var e = jQuery.Event('keydown');
      e.which = 13; // # Some key code value
      e.keyCode = 13;
      $('.jqconsole').trigger(e);
    });
  });
}());
