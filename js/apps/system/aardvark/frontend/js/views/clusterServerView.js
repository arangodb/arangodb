/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterServerView = Backbone.View.extend({
    
    el: '#clusterServers',

    template: templateEngine.createTemplate("clusterServerView.ejs"),

    events: {
      "click .server": "loadServer"
    },

    initialize: function() {
      this.dbView = new window.ClusterDatabaseView({
        collection: new window.ClusterDatabases()
      });
      this.isMinified = false;
    },

    loadServer: function(e) {
      var id = e.currentTarget.id;
      this.dbView.render(id);
      this.render(true);
    },

    stopUpdating: function() {
      this.dbView.stopUpdating();
    },

    unrender: function() {
      $(this.el).html("");
      this.dbView.unrender();
    },

    render: function(minify){
      this.isMinified = !!minify;
      if(!minify) {
        this.dbView.unrender();
      }
      $(this.el).html(this.template.render({
        minify: minify,
        servers: this.collection.getList()
      }));
      return this;
    }

  });

}());
