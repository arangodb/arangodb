/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect, runs, waitsFor*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Login Modal View', function () {
    var view, div;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'modalPlaceholder';
      document.body.appendChild(div);
      view = new window.LoginModalView();
      view.render();
      window.App = {
        isCheckingUser: true,
        clusterPlan: {
          storeCredentials: function () {
            throw 'Should be a spy';
          }
        }
      };
      spyOn(window.App.clusterPlan, 'storeCredentials');
    });

    afterEach(function () {
      document.body.removeChild(div);
      delete window.App;
    });

    it('should show the modal on render', function () {
      var fakeJQuery = {
          modal: function () {
            throw 'This should be a spy';
          }
        },
        called = false,
        orig$ = window.$;
      spyOn(window, '$').andCallFake(function (id) {
        if (id === '#loginModalLayer') {
          return fakeJQuery;
        }
        called = true;
        return orig$(id);
      });
      spyOn(fakeJQuery, 'modal');
      view.render();
      expect(fakeJQuery.modal).toHaveBeenCalledWith('show');
      expect(called).toBeTruthy();
    });

    it('should confirm login', function () {
      var uname = 'user',
        pw = 'secret';

      runs(function () {
        $('#username').val(uname);
        $('#password').val(pw);
        $('#confirmLogin').click();
        expect(window.App.clusterPlan.storeCredentials).toHaveBeenCalledWith(uname, pw);
      });

      waitsFor(function () {
        return !window.App.isCheckingUser;
      }, 'The modal should stop searching for a new user', 2000);

      runs(function () {
        expect($(div).html()).toEqual('');
      });
    });

    it('should be able to cancel', function () {
      runs(function () {
        $('.button-close').click();
      });

      waitsFor(function () {
        return !window.App.isCheckingUser;
      }, 'The modal should stop searching for a new user', 2000);

      runs(function () {
        expect(window.App.clusterPlan.storeCredentials).not.toHaveBeenCalled();
        expect($(div).html()).toEqual('');
      });
    });
  });
}());
