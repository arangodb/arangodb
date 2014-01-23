/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterDatabaseView = Backbone.View.extend({
    
    el: '#clusterDatabases',

    template: templateEngine.createTemplate("clusterDatabaseView.ejs"),

    events: {
      "click .database": "loadDatabase"
    },

    initialize: function() {
      this.colView = new window.ClusterCollectionView({
        collection: new window.ClusterCollections()
      });
      var rerender = function() {
        this.render(this.server);
      };
      this.collection.bind("add", rerender.bind(this));
      this.collection.bind("change", rerender.bind(this));
      this.collection.bind("remove", rerender.bind(this));
    },

    loadDatabase: function(e) {
      var id = e.currentTarget.id;
      this.colView.render(id, this.server);
    },

    unrender: function() {
      $(this.el).html("");
      this.colView.unrender();
    },

    stopUpdating: function() {
      this.collection.stopUpdating();
      this.colView.stopUpdating();
    },

    render: function(server) {
      this.server = server;
      this.collection.startUpdating();
      $(this.el).html(this.template.render({
        databases: this.collection.getList()
      }));
      this.colView.unrender();
      return this;
    }

  });

}());
