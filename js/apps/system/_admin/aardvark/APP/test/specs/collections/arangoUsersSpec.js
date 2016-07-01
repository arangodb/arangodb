/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect,
 require, jasmine,  exports, window, $ */
(function () {
  'use strict';

  describe('ArangoUsers', function () {
    var col;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be spy';
        }
      };
      col = new window.ArangoUsers();
      spyOn(window.App, 'navigate');
    });

    afterEach(function () {
      delete window.App;
    });

    it('checking defaults', function () {
      expect(col.model).toEqual(window.Users);
      expect(col.activeUser).toEqual('');
      expect(col.activeUserSettings).toEqual({
        'query': {},
        'shell': {},
        'testing': true
      });
      expect(col.url).toEqual('/_api/user');
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

      it('should check for user', function () {
        spyOn(a, 'get').andCallThrough();
        spyOn(b, 'get').andCallThrough();
        col.comparator(a, b);
        expect(a.get).toHaveBeenCalledWith('user');
        expect(b.get).toHaveBeenCalledWith('user');
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

    it('login', function () {
      col.login('user', 'pw');
      expect(col.activeUser).toEqual('user');
    });
    it('logout', function () {
      spyOn(col, 'reset');
      spyOn(window.location, 'reload');
      spyOn($, 'ajax').andCallFake(function (state, opt) {
        expect(state).toEqual('unauthorized');
        expect(opt).toEqual({async: false});
        return {
          error: function (callback) {
            callback();
          }
        };
      });
      col.logout();
      expect(col.activeUser).toEqual(undefined);
      expect(col.reset).toHaveBeenCalled();
      expect(window.App.navigate).toHaveBeenCalledWith('');
      expect(window.location.reload).toHaveBeenCalled();
    });

    it('setUserSettings', function () {
      col.setUserSettings('bad', true);
      expect(col.activeUserSettings.identifier).toEqual(true);
    });

    it('loadUserSettings with success', function () {
      col.activeUserSettings = {
        'query': {},
        'shell': {},
        'testing': true
      };
      col.activeUser = 'heinz';
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/user/' + encodeURIComponent(col.activeUser));
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success({extra: {
            Heinz: 'herbert',
            Heinz2: 'herbert2'
        }});
      });
      col.loadUserSettings();
      expect(col.activeUserSettings).toEqual({
        Heinz: 'herbert',
        Heinz2: 'herbert2'
      });
    });

    it('loadUserSettings with error', function () {
      col.activeUserSettings = {
        'query': {},
        'shell': {},
        'testing': true
      };
      col.activeUser = 'heinz';
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/user/' + encodeURIComponent(col.activeUser));
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error({extra: {
            Heinz: 'herbert',
            Heinz2: 'herbert2'
        }});
      });
      col.loadUserSettings();
      expect(col.activeUserSettings).toEqual({
        'query': {},
        'shell': {},
        'testing': true
      });
    });
    it('saveUserSettings with success', function () {
      col.activeUser = 'heinz';
      col.activeUserSettings = {a: 'B'};
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/user/' + encodeURIComponent(col.activeUser));
        expect(opt.type).toEqual('PUT');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({ extra: col.activeUserSettings }));
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success();
      });
      col.saveUserSettings();
      expect($.ajax).toHaveBeenCalled();
    });

    it('saveUserSettings with error', function () {
      col.activeUser = 'heinz';
      col.activeUserSettings = {a: 'B'};
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/user/' + encodeURIComponent(col.activeUser));
        expect(opt.type).toEqual('PUT');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({ extra: col.activeUserSettings }));
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error();
      });
      col.saveUserSettings();
      expect($.ajax).toHaveBeenCalled();
    });

    it('parse', function () {
      expect(col.parse({
        result: ['a', 'b', 'c']
      })).toEqual(['a', 'b', 'c']);
    });

    it('whoAmI without activeUser', function () {
      col.activeUserSettings = {a: 'B'};
      spyOn($, 'ajax').andCallFake(function (state, opt) {
        expect(state).toEqual('whoAmI');
        expect(opt).toEqual({async: false});
        return {
          done: function (callback) {
            callback({name: 'heinz'});
          }
        };
      });
      expect(col.whoAmI()).toEqual('heinz');
    });

    it('whoAmI with activeUser', function () {
      col.activeUser = 'heinz';
      expect(col.whoAmI()).toEqual('heinz');
    });
  });
}());
