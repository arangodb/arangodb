/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect*/
/* global templateEngine*/
(function () {
  'use strict';

  describe('Cluster Dashboard View', function () {
    var view, div, overview;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);
      overview = {
        render: function () {}
      };
      spyOn(window, 'ClusterOverviewView').andReturn(overview);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('initialization', function () {
      var servCol, coorCol;

      beforeEach(function () {
        servCol = {
          __id: 'servId'
        };
        coorCol = {
          __id: 'coorId'
        };
        spyOn(overview, 'render');
        spyOn(window, 'ClusterServers').andReturn(servCol);
        spyOn(window, 'ClusterCoordinators').andReturn(coorCol);
        view = new window.ClusterDashboardView();
      });

      it('should create the cluster server collection', function () {
        expect(window.ClusterServers).toHaveBeenCalled();
      });

      it('should create the cluster coordinators collection', function () {
        expect(window.ClusterCoordinators).toHaveBeenCalled();
      });

      describe('rendering', function () {
        var info;

        beforeEach(function () {
          window.ClusterServers.reset();
          window.ClusterCoordinators.reset();
        });

        it('should create a Cluster Overview View', function () {
          expect(window.ClusterOverviewView).not.toHaveBeenCalled();
          expect(window.ClusterServers).not.toHaveBeenCalled();
          expect(window.ClusterCoordinators).not.toHaveBeenCalled();
          window.ClusterOverviewView.reset();
          view.render();
          expect(window.ClusterOverviewView).toHaveBeenCalled();
          expect(window.ClusterServers).not.toHaveBeenCalled();
          expect(window.ClusterCoordinators).not.toHaveBeenCalled();
          window.ClusterOverviewView.reset();
          view.render();
          expect(window.ClusterOverviewView).toHaveBeenCalled();
          expect(window.ClusterServers).not.toHaveBeenCalled();
          expect(window.ClusterCoordinators).not.toHaveBeenCalled();
        });

        it('should render the Cluster Overview', function () {
          view.render();
          expect(overview.render).toHaveBeenCalled();
        });
      });
    });
  });
}());
