/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect, runs, waitsFor, jasmine*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Shutdown Button View', function () {
    var div;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'navigationBar';
      document.body.appendChild(div);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    it('should bind the overview', function () {
      var overview = {
          id: 'overview'
        },
        view = new window.ShutdownButtonView({
          overview: overview
        });
      expect(view.overview).toEqual(overview);
    });

    describe('rendered', function () {
      var view, overview;

      beforeEach(function () {
        overview = {
          stopUpdating: function () {
            throw 'This is should be a spy';
          }
        };
        window.App = {
          navigate: function () {
            throw 'This should be a spy';
          }
        };
        view = new window.ShutdownButtonView({
          overview: overview
        });
        view.render();
      });

      afterEach(function () {
        delete window.App;
      });

      it('should be able to shutdown the cluster', function () {
        spyOn(overview, 'stopUpdating');
        spyOn(window.App, 'navigate');
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('cluster/shutdown');
          expect(opt.cache).toEqual(false);
          expect(opt.type).toEqual('GET');
          expect(opt.success).toEqual(jasmine.any(Function));
          opt.success();
        });
        $('#clusterShutdown').click();
        expect(overview.stopUpdating).toHaveBeenCalled();
        expect(window.App.navigate).toHaveBeenCalledWith(
          'handleClusterDown', {trigger: true}
        );
      });

      it('should be able to unrender', function () {
        expect($(div).html()).not.toEqual('');
        view.unrender();
        expect($(div).html()).toEqual('');
      });
    });
  });
}());
