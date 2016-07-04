/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect,
 require, jasmine,  exports, window, $, arangoLog */
(function () {
  'use strict';

  describe('GraphCollection', function () {
    var col;

    beforeEach(function () {
      col = new window.GraphCollection();
    });

    it('parse', function () {
      expect(col.model).toEqual(window.Graph);
      expect(col.url).toEqual('/_api/gharial');
      expect(col.parse({error: false, graphs: 'blub'})).toEqual('blub');
      expect(col.parse({error: true, graphs: 'blub'})).toEqual(undefined);
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

      it('should check for _key', function () {
        spyOn(a, 'get').andCallThrough();
        spyOn(b, 'get').andCallThrough();
        col.comparator(a, b);
        expect(a.get).toHaveBeenCalledWith('_key');
        expect(b.get).toHaveBeenCalledWith('_key');
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
  });
}());
