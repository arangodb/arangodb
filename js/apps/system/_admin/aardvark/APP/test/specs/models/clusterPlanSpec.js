/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('The cluster Plan', function () {
    var cp;

    beforeEach(function () {
      cp = new window.ClusterPlan();
    });

    it('basics', function () {
      expect(cp.defaults).toEqual({});
      expect(cp.url).toEqual('cluster/plan');
      expect(cp.idAttribute).toEqual('config');
    });

    it('should getCoordinator', function () {
      spyOn(cp, 'get').andReturn([
        {},
        {
          isStartServers: true,
          endpoints: ['tcp://abcd.de', 'ssl://abcd.de'],
          roles: ['Coordinator', 'DBServer']
        },
        {
          isStartServers: false,
          endpoints: []
        }

      ]);
      cp.getCoordinator();
      expect(cp._coord).toEqual(['http://abcd.de']);
      cp.getCoordinator();
      expect(cp._coord).toEqual(['http://abcd.de']);
    });

    it('should getCoordinator with emoty runinfo', function () {
      spyOn(cp, 'get').andReturn(undefined);
      cp.getCoordinator();
      expect(cp._coord).toEqual(undefined);
    });

    it('isAlive with success', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('cluster/healthcheck');
        expect(opt.type).toEqual('GET');
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success('bla');
      });
      var result = cp.isAlive();
      expect(result).toEqual('bla');
    });

    it('isAlive with error', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('cluster/healthcheck');
        expect(opt.type).toEqual('GET');
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error('bla');
      });
      var result = cp.isAlive();
      expect(result).toEqual(false);
    });

    it('storeCredentials', function () {
      spyOn(cp, 'fetch');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('cluster/plan/credentials');
        expect(opt.type).toEqual('PUT');
        expect(opt.async).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({
          user: 'name',
          passwd: 'pw'
        }));
        return {
          done: function (f) {
            f();
          }
        };
      });
      var result = cp.storeCredentials('name', 'pw');
      expect(cp.fetch).toHaveBeenCalled();
    });

    it('isSymmetricSetup with asymmetrical setup', function () {
      spyOn(cp, 'get').andReturn({
        numberOfCoordinators: 2,
        numberOfDBservers: 3,
        dispatchers: [1, 2, 3, 4]
      });
      expect(cp.isSymmetricSetup()).toEqual(false);
    });

    it('isSymmetricSetup with symmetrical setup', function () {
      spyOn(cp, 'get').andReturn({
        numberOfCoordinators: 2,
        numberOfDBservers: 2,
        dispatchers: [1, 2]
      });
      expect(cp.isSymmetricSetup()).toEqual(true);
    });

    it('isTestSetup with test setup', function () {
      spyOn(cp, 'get').andReturn({
        dispatchers: [1]
      });
      expect(cp.isTestSetup()).toEqual(true);
    });

    it('isTestSetup with no test setup', function () {
      spyOn(cp, 'get').andReturn({
        dispatchers: [1, 2]
      });
      expect(cp.isTestSetup()).toEqual(false);
    });
  });
}());
