/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Collection View', function () {
    var view, div, shardsView, shardCol, server, db;

    beforeEach(function () {
      server = 'pavel';
      db = '_system';
      div = document.createElement('div');
      div.id = 'clusterCollections';
      document.body.appendChild(div);
      shardsView = {
        render: function () {},
        unrender: function () {}
      };
      shardCol = {
        __refId: 'testId'
      };
      spyOn(window, 'ClusterShardsView').andReturn(shardsView);
      spyOn(window, 'ClusterShards').andReturn(shardCol);
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('initialization', function () {
      it('should create a Cluster Server View', function () {
        view = new window.ClusterCollectionView();
        expect(window.ClusterShards).toHaveBeenCalled();
        expect(window.ClusterShardsView).toHaveBeenCalledWith({
          collection: shardCol
        });
      });
    });

    describe('rendering', function () {
      var docs, edges, people, colls, collCol,
        checkButtonContent = function (col, cls) {
          var btn = document.getElementById(col.name);
          expect(btn).toBeOfClass('btn');
          expect(btn).toBeOfClass('btn-server');
          expect(btn).toBeOfClass('collection');
          expect(btn).toBeOfClass('btn-' + cls);
          expect($(btn).text()).toEqual(col.name);
      };

      beforeEach(function () {
        docs = {
          name: 'Documents',
          status: 'ok'
        };
        edges = {
          name: 'Edges',
          status: 'warning'
        };
        people = {
          name: 'People',
          status: 'critical'
        };
        colls = [
          docs,
          edges,
          people
        ];
        collCol = {
          getList: function () {
            return colls;
          }
        };
        spyOn(shardsView, 'render');
        spyOn(shardsView, 'unrender');
        view = new window.ClusterCollectionView({
          collection: collCol
        });
        view.render(db, server);
      });

      it('should not unrender the server view', function () {
        expect(shardsView.render).not.toHaveBeenCalled();
        expect(shardsView.unrender).toHaveBeenCalled();
      });

      it('should render the documents collection', function () {
        checkButtonContent(docs, 'success');
      });

      it('should render the edge collection', function () {
        checkButtonContent(edges, 'warning');
      });

      it('should render the people collection', function () {
        checkButtonContent(people, 'danger');
      });

      it('should offer an unrender function', function () {
        shardsView.unrender.reset();
        view.unrender();
        expect($(div).html()).toEqual('');
        expect(shardsView.unrender).toHaveBeenCalled();
      });

      describe('user actions', function () {
        var info;

        beforeEach(function () {
          view = new window.ClusterCollectionView();
        });

        it('should be able to navigate to Documents', function () {
          $('#Documents').click();
          expect(shardsView.render).toHaveBeenCalledWith(db, 'Documents', server);
        });

        it('should be able to navigate to Edges', function () {
          $('#Edges').click();
          expect(shardsView.render).toHaveBeenCalledWith(db, 'Edges', server);
        });

        it('should be able to navigate to People', function () {
          $('#People').click();
          expect(shardsView.render).toHaveBeenCalledWith(db, 'People', server);
        });
      });
    });
  });
}());
