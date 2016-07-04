/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, Backbone, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('arangoDatabase', function () {
    var col;

    beforeEach(function () {
      // col = new window.ArangoDatabase()
      col = new window.ArangoDatabase(
        [],
        {shouldFetchUser: true}
      );
    });

    describe('comparator', function () {
      var a, b, c, first, second, third;

      beforeEach(function () {
        first = {value: ''};
        second = {value: ''};
        third = {value: ''};
        a = {
          get: function () { return first.value;}
        };
        b = {
          get: function () { return second.value;}
        };
        c = {
          get: function () { return third.value;}
        };
      });

      afterEach(function () {
        col.sortOptions.desc = false;
      });

      it('should check for name', function () {
        spyOn(a, 'get').andCallThrough();
        spyOn(b, 'get').andCallThrough();
        col.comparator(a, b);
        expect(a.get).toHaveBeenCalledWith('name');
        expect(b.get).toHaveBeenCalledWith('name');
      });

      it('should sort alphabetically', function () {
        col.sortOptions.desc = false;
        first.value = 'aaa';
        second.value = 'bbb';
        third.value = '_zzz';

        expect(col.comparator(a, a)).toEqual(0);
        expect(col.comparator(b, b)).toEqual(0);
        expect(col.comparator(c, c)).toEqual(0);

        expect(col.comparator(a, b)).toEqual(-1);
        expect(col.comparator(b, a)).toEqual(1);

        expect(col.comparator(a, c)).toEqual(1);
        expect(col.comparator(c, a)).toEqual(-1);

        expect(col.comparator(b, c)).toEqual(1);
        expect(col.comparator(c, b)).toEqual(-1);
      });

      it('should sort case independently', function () {
        col.sortOptions.desc = false;
        first.value = 'aaa';
        second.value = 'BBB';
        expect(col.comparator(a, b)).toEqual(-1);
        expect(col.comparator(b, a)).toEqual(1);
      });

      it('should allow to reverse order ignoring case', function () {
        col.sortOptions.desc = true;
        first.value = 'aaa';
        second.value = 'bbb';
        third.value = '_zzz';

        expect(col.comparator(a, a)).toEqual(0);
        expect(col.comparator(b, b)).toEqual(0);
        expect(col.comparator(c, c)).toEqual(0);

        expect(col.comparator(a, b)).toEqual(1);
        expect(col.comparator(b, a)).toEqual(-1);

        expect(col.comparator(a, c)).toEqual(-1);
        expect(col.comparator(c, a)).toEqual(1);

        expect(col.comparator(b, c)).toEqual(-1);
        expect(col.comparator(c, b)).toEqual(1);
      });

      it('should allow to reverse to counter alphabetical ordering', function () {
        col.sortOptions.desc = true;
        first.value = 'aaa';
        second.value = 'BBB';
        expect(col.comparator(a, b)).toEqual(1);
        expect(col.comparator(b, a)).toEqual(-1);
      });
    });

    it('url', function () {
      expect(col.url).toEqual('/_api/database');
    });

    it('parse undefined reponse', function () {
      expect(col.parse()).toEqual(undefined);
    });

    it('parse defined reponse', function () {
      expect(col.parse({result: {name: 'horst'}})).toEqual([
        {name: 'horst'}
      ]);
    });

    it('initialize', function () {
      spyOn(col, 'fetch').andCallFake(function () {
        col.models = [
          new window.DatabaseModel({name: 'heinz'}),
          new window.DatabaseModel({name: 'fritz'}),
          new window.DatabaseModel({name: 'anton'})
        ];
        return {
          done: function (a) {
            a();
          }
        };
      });
      var values = [],
        options = {shouldFetchUser: false};

      col.initialize(values, options);
      expect(col.models[0].get('name')).toEqual('anton');
      expect(col.models[1].get('name')).toEqual('fritz');
      expect(col.models[2].get('name')).toEqual('heinz');
    });

    it('getDatabases', function () {
      col.add(new window.DatabaseModel({name: 'heinz'}));
      col.add(new window.DatabaseModel({name: 'fritz'}));
      col.add(new window.DatabaseModel({name: 'anton'}));
      spyOn(col, 'fetch').andCallFake(function () {
        col.models = [
          new window.DatabaseModel({name: 'heinz'}),
          new window.DatabaseModel({name: 'fritz'}),
          new window.DatabaseModel({name: 'anton'})
        ];
        return {
          done: function (a) {
            a();
          }
        };
      });
      var result = col.getDatabases();
      expect(result[0].get('name')).toEqual('anton');
      expect(result[1].get('name')).toEqual('fritz');
      expect(result[2].get('name')).toEqual('heinz');
    });

    it('createDatabaseURL', function () {
      expect(col.createDatabaseURL('blub', 'SSL', 21)).toEqual(
        'https://localhost:21/_db/blub/_admin/aardvark/standalone.html'
      );
    });

    it('createDatabaseURL with http and application', function () {
      window.location.hash = '#application/abc';
      expect(col.createDatabaseURL('blub', 'http', 21)).toEqual(
        'http://localhost:21/_db/blub/_admin/aardvark/standalone.html#applications'
      );
    });

    it('createDatabaseURL with http and collection', function () {
      window.location.hash = '#collection/abc';
      expect(col.createDatabaseURL('blub', undefined, 21)).toEqual(
        'http://localhost:21/_db/blub/_admin/aardvark/standalone.html#collections'
      );
    });

    it('should getCurrentDatabase and succeed', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/database/current');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.processData).toEqual(false);
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success({code: 200, result: {name: 'eee'}});
      });
      var result = col.getCurrentDatabase();
      expect(result).toEqual('eee');
    });

    it('should getCurrentDatabase and succeed with 201', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/database/current');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.processData).toEqual(false);
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success({code: 201, result: {name: 'eee'}});
      });
      var result = col.getCurrentDatabase();
      expect(result).toEqual({code: 201, result: {name: 'eee'}});
    });

    it('should getCurrentDatabase and fail', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/database/current');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.processData).toEqual(false);
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error({code: 401, result: {name: 'eee'}});
      });
      var result = col.getCurrentDatabase();
      expect(result).toEqual({code: 401, result: {name: 'eee'}});
    });
  });
}());
