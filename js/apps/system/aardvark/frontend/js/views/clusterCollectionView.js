/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterCollectionView = Backbone.View.extend({
    
    el: '#clusterCollections',

    template: templateEngine.createTemplate("clusterCollectionView.ejs"),

    events: {
      "click .collection": "loadCollection"
    },

    initialize: function() {
      this.shardsView = new window.ClusterShardsView({
        collection: new window.ClusterShards()
      });
      var rerender = function() {
        this.render(this.db, this.server);
      };
      this.collection.bind("add", rerender.bind(this));
      this.collection.bind("change", rerender.bind(this));
      this.collection.bind("remove", rerender.bind(this));
    },

    loadCollection: function(e) {
      var id = e.currentTarget.id;
      this.shardsView.render(
        this.db,
        id,
        this.server
      );
    },

    unrender: function() {
      $(this.el).html("");
      this.shardsView.unrender();
    },

    stopUpdating: function() {
      this.collection.stopUpdating();
      this.shardsView.stopUpdating();
    },

    render: function(db, server) {
      this.db = db;
      this.server = server;
      this.collection.startUpdating();
      $(this.el).html(this.template.render({
        collections: this.collection.getList(this.db)
      }));
      this.shardsView.unrender();
      return this;
    }

  });

}());
