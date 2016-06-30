/* jshint browser: true */
/* jshint unused: false */
/* global $, arangoHelper, jasmine, nv, d3, describe, beforeEach, afterEach, it, spyOn, expect*/

(function () {
  'use strict';

  describe('The Show Shards View', function () {
    var view, getListDummy;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        },
        addAuth: {
          bind: function () {
            return 1;
          }
        }
      };

      getListDummy = {
        getList: function () {}
      };

      spyOn(window, 'ClusterServers').andReturn({
        fetch: function (a) {
          expect(a).toEqual({
            async: false,
            beforeSend: 1
          });
        }
      });
      spyOn(window, 'ClusterDatabases').andReturn(getListDummy);
      spyOn(window, 'ClusterCollections').andReturn(getListDummy);
      spyOn(window, 'ClusterShards').andReturn(getListDummy);
      view = new window.ShowShardsView();
      expect(window.ClusterServers).toHaveBeenCalledWith([], {
        interval: 10000
      });
      expect(window.ClusterDatabases).toHaveBeenCalledWith([], {
        interval: 10000
      });
      expect(window.ClusterCollections).toHaveBeenCalled();
      expect(window.ClusterShards).toHaveBeenCalledWith();
    });

    afterEach(function () {
      delete window.App;
    });

    it('assert the basics', function () {
      expect(view.events).toEqual({
        'change #selectDB': 'updateCollections',
        'change #selectCol': 'updateShards'
      });
    });

    it('assert updateCollections', function () {
      spyOn(view, 'updateShards');
      spyOn(getListDummy, 'getList').andReturn([
        {name: 'heinz'},
        {name: 'herbert'}
      ]);

      var jquerydummy = {
        find: function () {
          return jquerydummy;
        },

        attr: function () {},

        html: function () {},

        append: function () {}

      };
      view.template = {
        render: function () {
          return 'template';
        }
      };
      view.modal = {
        render: function () {
          return 'modal';
        }
      };
      spyOn(window, '$').andReturn(jquerydummy);
      spyOn(jquerydummy, 'find').andCallThrough();
      spyOn(jquerydummy, 'attr');
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'append');
      view.updateCollections();
      expect(window.$).toHaveBeenCalledWith('#selectDB');
      expect(window.$).toHaveBeenCalledWith('#selectCol');
      expect(jquerydummy.find).toHaveBeenCalledWith(':selected');
      expect(jquerydummy.attr).toHaveBeenCalledWith('id');
      expect(jquerydummy.html).toHaveBeenCalledWith('');
      expect(jquerydummy.append).toHaveBeenCalledWith(
        '<option id="' + 'heinz' + '">' + 'heinz' + '</option>'
      );
      expect(jquerydummy.append).toHaveBeenCalledWith(
        '<option id="' + 'herbert' + '">' + 'herbert' + '</option>'
      );
    });
    it('assert updateShards', function () {
      spyOn(getListDummy, 'getList').andReturn([
        {server: 'heinz', shards: [1, 2, 3]},
        {server: 'herbert', shards: [1, 2]}
      ]);

      var jquerydummy = {
        find: function () {
          return jquerydummy;
        },

        attr: function () {
          return 'x';
        },

        html: function () {},

        append: function () {},
        empty: function () {},

        toggleClass: function () {}

      };
      view.template = {
        render: function () {
          return 'template';
        }
      };
      view.modal = {
        render: function () {
          return 'modal';
        }
      };
      spyOn(window, '$').andReturn(jquerydummy);
      spyOn(jquerydummy, 'find').andCallThrough();
      spyOn(jquerydummy, 'attr').andCallThrough();
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'empty');
      spyOn(jquerydummy, 'toggleClass');
      spyOn(jquerydummy, 'append');
      view.updateShards();
      expect(window.$).toHaveBeenCalledWith('#selectDB');
      expect(window.$).toHaveBeenCalledWith('#selectCol');
      expect(window.$).toHaveBeenCalledWith('.shardContainer');
      expect(window.$).toHaveBeenCalledWith('#heinzShards');
      expect(window.$).toHaveBeenCalledWith('#herbertShards');
      expect(window.$).toHaveBeenCalledWith('.collectionName', jquerydummy);
      expect(getListDummy.getList).toHaveBeenCalledWith('x', 'x');
      expect(jquerydummy.find).toHaveBeenCalledWith(':selected');
      expect(jquerydummy.attr).toHaveBeenCalledWith('id');
      expect(jquerydummy.html).toHaveBeenCalledWith('heinz: 3');
      expect(jquerydummy.html).toHaveBeenCalledWith('herbert: 2');
      expect(jquerydummy.empty).toHaveBeenCalled();
    });

    it('assert render', function () {
      var jquerydummy = {
        html: function () {}
      };
      view.template = {
        render: function () {
          return 'template';
        }
      };
      spyOn(window, '$').andReturn(jquerydummy);
      spyOn(jquerydummy, 'html');
      spyOn(view, 'updateCollections');
      spyOn(view.template , 'render').andCallThrough();
      spyOn(getListDummy, 'getList').andReturn([{name: 'c'}, {name: 'd'}]);

      view.dbservers = {
        pluck: function () {
          return ['a', 'b'];
        }
      };
      view.render();
      expect(view.updateCollections).toHaveBeenCalled();
      expect(jquerydummy.html).toHaveBeenCalledWith('template');
      expect(view.template.render).toHaveBeenCalledWith({
        names: ['a', 'b'],
        dbs: ['c', 'd']
      });
    });
  });
}());
