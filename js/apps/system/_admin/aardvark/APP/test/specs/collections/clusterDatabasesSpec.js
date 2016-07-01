/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, jasmine, */
/* global spyOn, expect*/
/* global _*/
(function () {
  'use strict';

  describe('Cluster Databases Collection', function () {
    var col, list, db1, db2, db3;

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
      spyOn(window.App, 'getNewRoute').andCallFake(function (a) {
        return a;
      });
      spyOn(window.App, 'registerForUpdate');
      list = [];
      db1 = {name: '_system'};
      db2 = {name: 'otherDB'};
      db3 = {name: 'yaDB'};
      col = new window.ClusterDatabases();
      spyOn(col, 'fetch').andCallFake(function () {
        _.each(list, function (s) {
          col.add(s);
        });
      });
    });

    afterEach(function () {
      delete window.App;
    });

    describe('list', function () {
      it('should fetch the result sync', function () {
        col.fetch.reset();
        col.getList();
        expect(col.fetch).toHaveBeenCalledWith({
          async: false,
          beforeSend: jasmine.any(Function)
        });
      });

      it('should return a list with default ok status', function () {
        list.push(db1);
        list.push(db2);
        list.push(db3);
        var expected = [];
        expected.push({
          name: db1.name,
          status: 'ok'
        });
        expected.push({
          name: db2.name,
          status: 'ok'
        });
        expected.push({
          name: db3.name,
          status: 'ok'
        });
        expect(col.getList()).toEqual(expected);
      });

      it('updateUrl', function () {
        col.updateUrl();
        expect(col.url).toEqual('Databases');
      });
    });
  });
}());
