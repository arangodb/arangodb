/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Database View', function () {
    var view, div, colView, colCol, server;

    beforeEach(function () {
      server = 'pavel';
      div = document.createElement('div');
      div.id = 'clusterDatabases';
      document.body.appendChild(div);
      colView = {
        render: function () {},
        unrender: function () {}
      };
      colCol = {
        __id: 'refId'
      };
      spyOn(window, 'ClusterCollectionView').andReturn(colView);
      spyOn(window, 'ClusterCollections').andReturn(colCol);
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('initialization', function () {
      it('should create a Cluster Collection View', function () {
        view = new window.ClusterDatabaseView({
          collection: {}
        });
        expect(window.ClusterCollections).toHaveBeenCalled();
        expect(window.ClusterCollectionView).toHaveBeenCalledWith({
          collection: colCol
        });
      });
    });

    describe('rendering', function () {
      var db1, db2, db3, databases, dbCol,
        checkButtonContent = function (db, cls) {
          var btn = document.getElementById(db.name);
          expect(btn).toBeOfClass('btn');
          expect(btn).toBeOfClass('btn-server');
          expect(btn).toBeOfClass('database');
          expect(btn).toBeOfClass('btn-' + cls);
          expect($(btn).text()).toEqual(db.name);
      };

      beforeEach(function () {
        spyOn(colView, 'render');
        spyOn(colView, 'unrender');
        db1 = {
          name: '_system',
          status: 'ok'
        };
        db2 = {
          name: 'myDatabase',
          status: 'warning'
        };
        db3 = {
          name: 'otherDatabase',
          status: 'critical'
        };
        databases = [
          db1,
          db2,
          db3
        ];
        dbCol = {
          getList: function () {
            return databases;
          }
        };
        view = new window.ClusterDatabaseView({
          collection: dbCol
        });
        view.render(server);
      });

      it('should not render the Server view', function () {
        expect(colView.render).not.toHaveBeenCalled();
        expect(colView.unrender).toHaveBeenCalled();
      });

      it('should render the ok database', function () {
        checkButtonContent(db1, 'success');
      });

      it('should render the warning database', function () {
        checkButtonContent(db2, 'warning');
      });

      it('should render the error database', function () {
        checkButtonContent(db3, 'danger');
      });

      it('should offer an unrender function', function () {
        colView.unrender.reset();
        view.unrender();
        expect($(div).html()).toEqual('');
        expect(colView.unrender).toHaveBeenCalled();
      });

      describe('user actions', function () {
        var db;

        it('should be able to navigate to _system', function () {
          db = '_system';
          $('#' + db).click();
          expect(colView.render).toHaveBeenCalledWith(db, server);
        });

        it('should be able to navigate to myDatabase', function () {
          db = 'myDatabase';
          $('#' + db).click();
          expect(colView.render).toHaveBeenCalledWith(db, server);
        });

        it('should be able to navigate to otherDatabase', function () {
          db = 'otherDatabase';
          $('#' + db).click();
          expect(colView.render).toHaveBeenCalledWith(db, server);
        });
      });
    });
  });
}());
