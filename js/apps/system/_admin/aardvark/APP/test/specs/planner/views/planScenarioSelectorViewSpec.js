/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global runs, waitsFor, jasmine*/
/* global $*/
(function () {
  'use strict';

  describe('PlanScenarioSelector View', function () {
    var myView;

    beforeEach(function () {
      $('body').append('<div id="content" class="removeMe"></div>');
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        }
      };
      spyOn(window.App, 'navigate');
      myView = new window.PlanScenarioSelectorView();
      myView.render();
    });

    afterEach(function () {
      $('.removeMe').remove();
      delete window.App;
    });

    describe('select scenario', function () {
      it('multiServerSymmetrical', function () {
        $('#multiServerSymmetrical').click();
        expect(window.App.navigate).toHaveBeenCalledWith('planSymmetrical', {trigger: true});
      });

      it('multiServerAsymmetrical', function () {
        $('#multiServerAsymmetrical').click();
        expect(window.App.navigate).toHaveBeenCalledWith('planAsymmetrical', {trigger: true});
      });
      it('singleServer', function () {
        $('#singleServer').click();
        expect(window.App.navigate).toHaveBeenCalledWith('planTest', {trigger: true});
      });
    });
  });
}());
