/* jshint browser: true */
/* jshint unused: false */
/* global $, arangoHelper, jasmine, nv, d3, describe, beforeEach, afterEach, it, spyOn, expect*/

(function () {
  'use strict';

  describe('The ServerDashboardView', function () {
    var view;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        },
        showClusterView: {
          startUpdating: function () {}
        }
      };
      window.modalView = {
        show: function () {}

      };
      window.DashboardView = {
        prototype: {
          render: {
            bind: function () {
              return function () {};
            }
          }
        }
      };
      view = new window.ServerDashboardView();
    });

    afterEach(function () {
      delete window.App;
    });

    it('assert render', function () {
      var jquerydummy = {
        toggleClass: function () {
          return this;
        },
        on: function (a, b) {
          expect(a).toEqual('hidden');
          b();
        },
        attr: function () {
          return jquerydummy;
        },
        append: function () {
          return undefined;
        }
      };
      spyOn(view, 'hide');
      spyOn(window, '$').andReturn(jquerydummy);
      spyOn(window.modalView, 'show');
      spyOn(jquerydummy, 'toggleClass').andCallThrough();
      spyOn(jquerydummy, 'on').andCallThrough();
      spyOn(jquerydummy, 'attr').andCallThrough();

      view.render();

      expect(view.hide).toHaveBeenCalled();
      expect(window.modalView.hideFooter).toEqual(false);

      expect(window.modalView.show).toHaveBeenCalledWith('dashboardView.ejs',
        null,
        undefined,
        undefined,
        undefined,
        view.events);
      expect(jquerydummy.toggleClass).toHaveBeenCalledWith('modal-chart-detail', true);
      expect(jquerydummy.attr).toHaveBeenCalledWith('data-dismiss', 'modal');
      expect(jquerydummy.attr).toHaveBeenCalledWith('aria-hidden', 'true');
      expect(jquerydummy.attr).toHaveBeenCalledWith('type', 'button');
    });

    it('assert hide', function () {
      spyOn(window.App.showClusterView, 'startUpdating');
      spyOn(view, 'stopUpdating');
      view.hide();
      expect(window.App.showClusterView.startUpdating).toHaveBeenCalled();
      expect(view.stopUpdating).toHaveBeenCalled();
    });
  });
}());
