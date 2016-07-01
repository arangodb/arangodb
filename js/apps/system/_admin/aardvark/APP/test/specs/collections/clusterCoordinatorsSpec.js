/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, jasmine, expect*/
/* global $*/

(function () {
  'use strict';

  describe('ClusterCoordinators', function () {
    var col;

    beforeEach(function () {
      window.App = {
        addAuth: function () {
          throw 'This should be a spy';
        },
        getNewRoute: function () {
          throw 'This should be a spy';
        },
        registerForUpdate: function () {
          throw 'This should be a spy';
        }
      };
      spyOn(window.App, 'addAuth');
      spyOn(window.App, 'getNewRoute');
      spyOn(window.App, 'registerForUpdate');
      col = new window.ClusterCoordinators();
    });

    afterEach(function () {
      delete window.App;
    });

    it('ClusterCoordinators', function () {
      expect(col.model).toEqual(window.ClusterCoordinator);
      expect(col.url).toEqual('/_admin/aardvark/cluster/Coordinators');
    });
    it('updateUrl', function () {
      col.updateUrl();
      expect(window.App.getNewRoute).toHaveBeenCalledWith('Coordinators');
    });
    it('statusClass', function () {
      expect(col.statusClass('ok')).toEqual('success');
      expect(col.statusClass('warning')).toEqual('warning');
      expect(col.statusClass('critical')).toEqual('danger');
      expect(col.statusClass('missing')).toEqual('inactive');
      expect(col.statusClass('a')).toEqual(undefined);
    });

    it('getStatuses', function () {
      var m = new window.ClusterCoordinator({name: 'a',
        status: 'undefined', address: 'localhost:8529'}),
        m1 = new window.ClusterCoordinator({name: 'b',
        status: 'critical', address: 'localhost:8529'}),
        m2 = new window.ClusterCoordinator({name: 'c',
        status: 'warning', address: 'localhost:8529'}),
        m3 = new window.ClusterCoordinator({name: 'd',
        status: 'ok', address: 'localhost:8529'}),
        self = this;
      col.add(m);
      col.add(m1);
      col.add(m2);
      col.add(m3);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
        return {
          done: function (cb) {
            cb();
          }
        };
      }
      );
      this.res = [];
      col.getStatuses(function (a, b) {
        self.res.push(a + b);
      });
      expect(this.res).toEqual(
        ['undefinedlocalhost:8529', 'dangerlocalhost:8529',
          'warninglocalhost:8529', 'successlocalhost:8529']);
    });

    it('byAddress', function () {
      var m = new window.ClusterCoordinator(
        {status: 'ok', address: 'localhost:8529'});
      col.add(m);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.byAddress({})).toEqual({
        localhost: {
          coords: [m]
        }
      });
    });

    it('getList', function () {
      var m = new window.ClusterCoordinator(
        {status: 'ok', address: 'localhost:8529'});
      col.add(m);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getList({})).toEqual([{
        name: m.get('name'),
        status: m.get('status'),
        url: m.get('url')
      }]);
    });

    it('getOverview with ok', function () {
      var m = new window.ClusterCoordinator(
        {status: 'ok', address: 'localhost:8529'});
      col.add(m);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getOverview({})).toEqual({
        plan: 1,
        having: 1,
        status: 'ok'
      });
    });

    it('getOverview with critical', function () {
      var m = new window.ClusterCoordinator(
        {status: 'critical', address: 'localhost:8529'});
      col.add(m);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getOverview({})).toEqual({
        plan: 1,
        having: 0,
        status: 'critical'
      });
    });

    it('getOverview with warning', function () {
      var m = new window.ClusterCoordinator({
      status: 'warning', address: 'localhost:8529'});
      col.add(m);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getOverview({})).toEqual({
        plan: 1,
        having: 1,
        status: 'warning'
      });
    });

    it('getOverview with undefined state', function () {
      var m = new window.ClusterCoordinator({name: 'a',
        status: 'undefined', address: 'localhost:8529'}),
        m1 = new window.ClusterCoordinator({name: 'b',
        status: 'critical', address: 'localhost:8529'}),
        m2 = new window.ClusterCoordinator({name: 'c',
        status: 'warning', address: 'localhost:8529'}),
        m3 = new window.ClusterCoordinator({name: 'd',
        status: 'ok', address: 'localhost:8529'});
      col.add(m);
      col.add(m1);
      col.add(m2);
      col.add(m3);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getOverview({})).toEqual({
        plan: 4, having: 2, status: 'critical'
      });
    });
  });
}());
