/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Shard View', function () {
    var view, div;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'clusterShards';
      document.body.appendChild(div);
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('rendering', function () {
      var s1, s2, s3, shards, shardCol,
        checkButtonContent = function (sh, cls) {
          var btn = document.getElementById(sh.id);
          expect(btn).toBeOfClass('btn');
          expect(btn).toBeOfClass('btn-server');
          expect(btn).toBeOfClass('shard');
          expect(btn).toBeOfClass('btn-' + cls);
          expect($(btn).text()).toEqual(sh.id);
      };

      beforeEach(function () {
        s1 = {
          id: 'Shard 1',
          status: 'ok'
        };
        s2 = {
          id: 'Shard 2',
          status: 'warning'
        };
        s3 = {
          id: 'Shard 3',
          status: 'critical'
        };
        shards = [
          s1,
          s2,
          s3
        ];
        shardCol = {
          getList: function () {
            return shards;
          }
        };
        view = new window.ClusterShardsView({
          collection: shardCol
        });
        view.render();
      });

      it('should render the first shard', function () {
        checkButtonContent(s1, 'success');
      });

      it('should render the second shard', function () {
        checkButtonContent(s2, 'warning');
      });

      it('should render the third shard', function () {
        checkButtonContent(s3, 'danger');
      });

      it('should offer an unrender function', function () {
        view.unrender();
        expect($(div).html()).toEqual('');
      });
    });
  });
}());
