/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect, runs, waitsFor*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Plan Selection View', function () {
    var view, div;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);
      view = new window.PlanScenarioSelectorView();
      view.render();
      window.App = {
        navigate: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(window.App, 'navigate');
    });

    afterEach(function () {
      document.body.removeChild(div);
      delete window.App;
    });

    it('should offer a single-machine plan', function () {
      $('#singleServer').click();
      expect(window.App.navigate).toHaveBeenCalledWith('planTest', {trigger: true});
    });

    it('should offer a multi-machine plan', function () {
      $('#multiServerAsymmetrical').click();
      expect(window.App.navigate).toHaveBeenCalledWith('planAsymmetrical', {trigger: true});
    });
  });
}());
