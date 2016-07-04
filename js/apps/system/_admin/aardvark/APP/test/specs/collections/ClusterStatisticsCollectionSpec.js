/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect,
 require, jasmine,  exports, window */
(function () {
  'use strict';

  describe('ClusterStatisticsCollection', function () {
    var col;

    beforeEach(function () {
      var clusterPlan = {
        getCoordinator: function () {
          return 'fritz';
        }
      };
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
      spyOn(window.App, 'getNewRoute').andCallFake(function (last) {
        if (last === 'statistics') {
          return clusterPlan.getCoordinator()
            + '/_admin/'
            + last;
        }
        return clusterPlan.getCoordinator()
          + '/_admin/aardvark/cluster/'
          + last;
      });
      spyOn(window.App, 'registerForUpdate').andCallFake(function (o) {
        o.updateUrl();
      });
      spyOn(window.App, 'addAuth');
      col = new window.ClusterStatisticsCollection();
    });

    afterEach(function () {
      delete window.App;
    });

    it('inititalize', function () {
      expect(col.model).toEqual(window.Statistics);
      expect(col.url).toEqual('fritz/_admin/statistics');
      spyOn(col, 'updateUrl').andCallThrough();
      col.initialize();
      expect(col.updateUrl).toHaveBeenCalled();
    });

  /* TODO fix
  it("fetch", function () {
    var m1 = new window.Statistics(),m2 = new window.Statistics()
    spyOn(m1, "fetch")
    spyOn(m2, "fetch")
    col.add(m1)
    col.add(m2)
    col.fetch()
    expect(m1.fetch).toHaveBeenCalledWith({
      async: false,
      beforeSend: jasmine.any(Function)
    })
    expect(m2.fetch).toHaveBeenCalledWith({
      async: false,
      beforeSend: jasmine.any(Function)
    })
  })
  */
  });
}());
