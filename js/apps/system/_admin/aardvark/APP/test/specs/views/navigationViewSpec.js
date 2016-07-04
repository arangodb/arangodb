/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/* global $*/

(function () {
  'use strict';

  describe('The navigation bar', function () {
    var div, view, currentDBFake, curName, isSystem,
      DBSelectionViewDummy, StatisticBarViewDummy, UserBarViewDummy,
      NotificationViewDummy, UserCollectionDummy, oldRouter;

    beforeEach(function () {
      curName = '_system';
      isSystem = true;
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        }
      };
      spyOn(window.App, 'navigate');

      window.currentDB = window.currentDB || {
        get: function () {}
      };

      DBSelectionViewDummy = {
        id: 'DBSelectionViewDummy',
        render: function () {}
      };

      UserBarViewDummy = {
        id: 'UserBarViewDummy',
        render: function () {}
      };

      StatisticBarViewDummy = {
        id: 'StatisticBarViewDummy',
        render: function () {}
      };

      NotificationViewDummy = {
        id: 'NotificationViewDummy',
        render: function () {}
      };

      UserCollectionDummy = {
        id: 'UserCollectionDummy',
        whoAmI: function () {
          return 'root';
        }
      };

      spyOn(window, 'UserBarView').andReturn(UserBarViewDummy);
      spyOn(window, 'StatisticBarView').andReturn(StatisticBarViewDummy);
      spyOn(window, 'DBSelectionView').andReturn(DBSelectionViewDummy);
      spyOn(window, 'NotificationView').andReturn(NotificationViewDummy);
      spyOn(window.currentDB, 'get').andCallFake(function (key) {
        if (key === 'name') {
          return curName;
        }
        if (key === 'isSystem') {
          return isSystem;
        }
        expect(true).toBeFalsy();
      });
      div = document.createElement('div');
      div.id = 'navigationBar';
      document.body.appendChild(div);
    });

    afterEach(function () {
      document.body.removeChild(div);
      delete window.App;
    });

    describe('in any database', function () {
      beforeEach(function () {
        view = new window.NavigationView(
          {userCollection: UserCollectionDummy, currentDB: window.currentDB}
        );
        view.render();
      });

      it('should offer a collections tab', function () {
        var tab = $('#collections', $(div));
        expect(tab.length).toEqual(1);
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith('collections', {trigger: true});
      });

      it('should offer a applications tab', function () {
        var tab = $('#applications', $(div));
        expect(tab.length).toEqual(1);
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith('applications', {trigger: true});
      });

      it('should offer a graph tab', function () {
        var tab = $('#graph', $(div));
        expect(tab.length).toEqual(1);
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith('graph', {trigger: true});
      });

      it('should offer an aql editor tab', function () {
        var tab = $('#query', $(div));
        expect(tab.length).toEqual(1);
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith('query', {trigger: true});
      });

      it('should offer an api tab', function () {
        var tab = $('#api', $(div));
        expect(tab.length).toEqual(1);
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith('api', {trigger: true});
      });

      it('should offer a graph tab', function () {
        var tab = $('#graph', $(div));
        expect(tab.length).toEqual(1);
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith('graph', {trigger: true});
      });

      it('should handle selected database render', function () {
        spyOn(view.dbSelectionView, 'render');
        view.handleSelectDatabase();
        expect(view.dbSelectionView.render).toHaveBeenCalled();
      });

      it('should navigate to the selected value from options div', function () {
        var toNavigate = '#collections';
        $('#arangoCollectionSelect').val(toNavigate);
        view.navigateBySelect();
        expect(window.App.navigate).toHaveBeenCalledWith(toNavigate, {trigger: true});
      });

      it('should navigate automatic to the selected value from options div', function () {
        spyOn($.fn, 'change').andCallFake(function (a) {
          a();
        });
        spyOn(view, 'navigateBySelect');
        view.handleSelectNavigation();
        expect($.fn.change).toHaveBeenCalledWith(jasmine.any(Function));
        expect(view.navigateBySelect).toHaveBeenCalled();
      });

      it('should render selectMenuItems correctly', function () {
        var entry = 'tools-menu',
          toBeActiveClass = $('.' + entry),
          toBeFalseClass1 = $('.' + 'graphviewer-menu'),
          toBeFalseClass2 = $('.' + 'databases-menu'),
          toBeFalseClass3 = $('.' + 'query-menu'),
          toBeFalseClass4 = $('.' + 'collections-menu'),
          toBeFalseClass5 = $('.' + 'applications-menu');
        view.selectMenuItem(entry);
        expect(toBeActiveClass.hasClass('active')).toBeTruthy();
        expect(toBeFalseClass1.hasClass('active')).toBeFalsy();
        expect(toBeFalseClass2.hasClass('active')).toBeFalsy();
        expect(toBeFalseClass3.hasClass('active')).toBeFalsy();
        expect(toBeFalseClass4.hasClass('active')).toBeFalsy();
        expect(toBeFalseClass5.hasClass('active')).toBeFalsy();
      });

      it('should show dropdown for menu item: links', function () {
        var e = {
          target: {
            id: 'links'
          }
        };
        spyOn($.fn, 'show');
        view.showDropdown(e);
        expect($.fn.show).toHaveBeenCalledWith(200);
      });

      it('should show dropdown for menu item: tools', function () {
        var e = {
          target: {
            id: 'tools'
          }
        };
        spyOn($.fn, 'show');
        view.showDropdown(e);
        expect($.fn.show).toHaveBeenCalledWith(200);
      });

      it('should show dropdown for menu item: dbselection', function () {
        var e = {
          target: {
            id: 'dbselection'
          }
        };
        spyOn($.fn, 'show');
        view.showDropdown(e);
        expect($.fn.show).toHaveBeenCalledWith(200);
      });

      it('should hide dropdown for menu item: linkDropdown', function () {
        spyOn($.fn, 'hide');
        $('#linkDropdown').mouseenter().mouseleave();
        expect($.fn.hide).toHaveBeenCalled();
      });

      it('should hide dropdown for menu item: toolsDropdown', function () {
        spyOn($.fn, 'hide');
        $('#toolsDropdown').mouseenter().mouseleave();
        expect($.fn.hide).toHaveBeenCalled();
      });

      it('should hide dropdown for menu item: dbSelect', function () {
        spyOn($.fn, 'hide');
        $('#dbSelect').mouseenter().mouseleave();
        expect($.fn.hide).toHaveBeenCalled();
      });

      it('should navigateByTab: tools', function () {
        spyOn($.fn, 'slideToggle');
        $('#tools').click();
        expect($.fn.slideToggle).toHaveBeenCalled();
      });

      it('should navigateByTab: links', function () {
        spyOn($.fn, 'slideToggle');
        $('#links').click();
        expect($.fn.slideToggle).toHaveBeenCalled();
      });

      it('should navigateByTab: dbSelection', function () {
        $('#tools').attr('id', 'dbselection');
        spyOn($.fn, 'slideToggle');
        $('#dbselection').click();
        expect($.fn.slideToggle).toHaveBeenCalled();
        $('#dbselection').attr('id', 'tools');
      });

      it('should navigateByTab: blank', function () {
        $('#tools').attr('id', '');
        $('#links').attr('id', '');
        spyOn($.fn, 'attr');
        $('.tab').click();
        expect($.fn.attr).toHaveBeenCalled();
      });
    });

    describe('in _system database', function () {
      beforeEach(function () {
        view = new window.NavigationView(
          {userCollection: UserCollectionDummy, currentDB: window.currentDB}
        );
        view.render();
      });

      it('should offer a logs tab', function () {
        var tab = $('#logs', $(div));
        expect(tab.length).toEqual(1);
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith('logs', {trigger: true});
      });
    });

    describe('in a not _system database', function () {
      beforeEach(function () {
        curName = 'firstDB';
        isSystem = false;
        view = new window.NavigationView(
          {userCollection: UserCollectionDummy, currentDB: window.currentDB}
        );
        view.render();
      });

      it('should not offer a logs tab', function () {
        var tab = $('#logs', $(div));
        expect(tab.length).toEqual(0);
      });
    });
  });
}());
