/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, jasmine, */
/* global spyOn, expect*/
/* global _*/
(function () {
  'use strict';

  describe('Cluster Collections Collection', function () {
    var col, list, c1, c2, c3;

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
      list = [];
      c1 = {name: 'Documents', id: '123'};
      c2 = {name: 'Edges', id: '321'};
      c3 = {name: '_graphs', id: '222'};
      col = new window.ClusterCollections();
      spyOn(col, 'fetch').andCallFake(function () {
        _.each(list, function (s) {
          col.add(s);
        });
      });
    });

    afterEach(function () {
      delete window.App;
    });

    describe('list overview', function () {
      it('should fetch the result sync', function () {
        col.fetch.reset();
        col.getList();
        expect(col.fetch).toHaveBeenCalledWith({
          async: false,
          beforeSend: jasmine.any(Function)
        });
      });

      it('url', function () {
        col.dbname = 'db';
        expect(col.url()).toEqual('/_admin/aardvark/cluster/'
          + 'db/'
          + 'Collections');
      });

      it('stopUpdating', function () {
        col.timer = 4;
        spyOn(window, 'clearTimeout');
        col.stopUpdating();
        expect(col.isUpdating).toEqual(false);
        expect(window.clearTimeout).toHaveBeenCalledWith(4);
      });

      it('startUpdating while already updating', function () {
        col.isUpdating = true;
        spyOn(window, 'setInterval');
        col.startUpdating();
        expect(window.setInterval).not.toHaveBeenCalled();
      });

      it('startUpdating while', function () {
        col.isUpdating = false;
        spyOn(window, 'setInterval').andCallFake(function (a) {
          a();
        });
        col.startUpdating();
        expect(window.setInterval).toHaveBeenCalled();
      });

      it('should return a list with default ok status', function () {
        list.push(c1);
        list.push(c2);
        list.push(c3);
        var expected = [];
        expected.push({
          name: c1.name,
          status: 'ok'
        });
        expected.push({
          name: c2.name,
          status: 'ok'
        });
        expected.push({
          name: c3.name,
          status: 'ok'
        });
        expect(col.getList()).toEqual(expected);
      });
    });
  });
}());
