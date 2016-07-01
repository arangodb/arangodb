/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, jasmine, */
/* global spyOn, expect*/
/* global templateEngine, $, _, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Servers Collection', function () {
    var col;

    beforeEach(function () {
      window.App = {
        addAuth: function () {
          throw 'This should be a spy';
        },
        getNewRoute: function () {
          throw 'This should be a spy';
        },
        requestAuth: function () {
          throw 'This should be a spy';
        },
        registerForUpdate: function () {
          throw 'This should be a spy';
        }
      };
      spyOn(window.App, 'addAuth');
      spyOn(window.App, 'requestAuth');
      spyOn(window.App, 'getNewRoute');
      spyOn(window.App, 'registerForUpdate');
      col = new window.ClusterServers();
    });

    afterEach(function () {
      delete window.App;
    });

    it('updateUrl', function () {
      col.updateUrl();
      expect(window.App.getNewRoute).toHaveBeenCalledWith('DBServers');
    });
    it('statusClass', function () {
      expect(col.statusClass('ok')).toEqual('success');
      expect(col.statusClass('warning')).toEqual('warning');
      expect(col.statusClass('critical')).toEqual('danger');
      expect(col.statusClass('missing')).toEqual('inactive');
      expect(col.statusClass('a')).toEqual(undefined);
    });

    it('getStatuses', function () {
      var m = new window.ClusterServer({name: 'a',
        status: 'undefined', address: 'localhost:8529'}),
        m1 = new window.ClusterServer({name: 'b',
        status: 'critical', address: 'localhost:8529'}),
        m2 = new window.ClusterServer({name: 'c',
        status: 'warning', address: 'localhost:8529'}),
        m3 = new window.ClusterServer({name: 'd',
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
            return {
              fail: function (a) {
                a({status: 401});
              }

            };
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

    /* getStatuses: function(cb) {
         var self = this,
             completed = function() {
                 self.forEach(function(m) {
                     cb(self.statusClass(m.get("status")), m.get("address"))
                 })
             }
         // This is the first function called in
         // Each update loop
         this.fetch({
             async: false,
             beforeSend: window.App.addAuth.bind(window.App)
         }).done(completed)
             .fail(function(d) {
                 if (d.status === 401) {
                     window.App.requestAuth()
                 }
             })
     },
*/

    it('byAddress', function () {
      var m = new window.ClusterServer({status: 'ok', address: 'localhost:8529'});
      col.add(m);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.byAddress({})).toEqual({
        localhost: {
          dbs: [m]
        }
      });
    });

    it('getOverview with ok', function () {
      var m = new window.ClusterServer({status: 'ok',
      address: 'localhost:8529', role: 'primary'});
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
      var m = new window.ClusterServer({status: 'critical',
      address: 'localhost:8529', role: 'primary'});
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
      var m = new window.ClusterServer({status: 'warning',
      address: 'localhost:8529', role: 'primary'});
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
      var m = new window.ClusterServer({name: 'a',
        status: 'undefined', address: 'localhost:8529', role: 'primary'}),
        m2 = new window.ClusterServer({name: 'c',
        status: 'warning', address: 'localhost:8529', role: 'primary'}),
        m3 = new window.ClusterServer({name: 'd',
        status: 'ok', address: 'localhost:8529', role: 'primary'}),
        mc1 = new window.ClusterServer({name: 'b',
        status: 'critical', address: 'localhost:8529', role: 'primary', secondary: m3}),
        mc2 = new window.ClusterServer({name: 'b',
        status: 'critical', address: 'localhost:8529', role: 'primary', secondary: mc1}),
        mc3 = new window.ClusterServer({name: 'b',
        status: 'critical', address: 'localhost:8529', role: 'primary'});

      col.add(m);
      col.add(mc1);
      col.add(mc2);
      col.add(mc3);
      col.add(m2);
      col.add(m3);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getOverview({})).toEqual({ plan: 4, having: 3, status: 'warning' });
    });

    it('getOverview with very critical state', function () {
      var m = new window.ClusterServer({name: 'a',
        status: 'undefined', address: 'localhost:8529', role: 'primary'}),
        m2 = new window.ClusterServer({name: 'c',
        status: 'warning', address: 'localhost:8529', role: 'primary'}),
        m3 = new window.ClusterServer({name: 'd',
        status: 'ok', address: 'localhost:8529', role: 'primary'}),
        mc1 = new window.ClusterServer({name: 'b',
        status: 'critical', address: 'localhost:8529', role: 'primary'}),
        mc2 = new window.ClusterServer({name: 'b',
        status: 'critical', address: 'localhost:8529', role: 'primary'}),
        mc3 = new window.ClusterServer({name: 'b',
        status: 'critical', address: 'localhost:8529', role: 'primary'});

      col.add(m);
      col.add(mc1);
      col.add(mc2);
      col.add(mc3);
      col.add(m2);
      col.add(m3);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getOverview({})).toEqual({ plan: 4, having: 2, status: 'critical' });
    });

    it('getList', function () {
      var m = new window.ClusterServer({name: 'a',
        status: 'ok', address: 'localhost:8529', role: 'primary'}),
        m2 = new window.ClusterServer({name: 'b',
        status: 'ok', address: 'localhost:8529', role: 'primary', secondary: m});
      col.add(m);
      col.add(m2);
      spyOn(col, 'fetch').andCallFake(function (param) {
        expect(param.async).toEqual(false);
      });
      expect(col.getList({})).toEqual([ { primary: { name: 'a', address: 'localhost:8529', status: 'ok' } },
        { primary: { name: 'b', address: 'localhost:8529', status: 'ok' },
        secondary: { name: 'a', address: 'localhost:8529', status: 'ok' } } ]);
    });
  });
}());
