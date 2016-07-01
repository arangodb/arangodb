/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, jasmine, */
/* global spyOn, expect*/
/* global _*/
(function () {
  'use strict';

  describe('ClusterShards Collection', function () {
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
      c1 = {name: 'Documents', shards: '123'};
      c2 = {name: 'Edges', shards: '321'};
      c3 = {name: '_graphs', shards: '222'};
      col = new window.ClusterShards();
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
        col.colname = 'col';
        expect(col.url()).toEqual('/_admin/aardvark/cluster/'
          + col.dbname + '/'
          + col.colname + '/'
          + 'Shards');
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
          server: c1.name,
          shards: c1.shards
        });
        expected.push({
          server: c2.name,
          shards: c2.shards
        });
        expected.push({
          server: c3.name,
          shards: c3.shards
        });
        expect(col.getList()).toEqual(expected);
      });
    });
  });
}());
