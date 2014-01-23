/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterShardsView = Backbone.View.extend({
    
    el: '#clusterShards',

    template: templateEngine.createTemplate("clusterShardsView.ejs"),

    initialize: function() {
      var rerender = function() {
        if ($(this.el).html().trim() !== "") {
          this.render(this.db, this.col, this.server);
        }
      };
      this.collection.bind("add", rerender.bind(this));
      this.collection.bind("change", rerender.bind(this));
      this.collection.bind("remove", rerender.bind(this));
    },

    unrender: function() {
      $(this.el).html("");
    },

    stopUpdating: function() {
      this.collection.stopUpdating();
    },

    render: function(db, col, server) {
      this.db = db;
      this.col = col;
      this.server = server;
      this.collection.startUpdating();
      $(this.el).html(this.template.render({
        shards: this.collection.getList(db, col, server)
      }));
      return this;
    }

  });

}());

