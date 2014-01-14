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
      this.dbView = new window.ClusterDatabaseView();
    },

    loadServer: function(e) {
      var id = e.currentTarget.id;
      this.dbView.render({
        name: id
      });
    },

    render: function(){
      $(this.el).html(this.template.render({}));
      return this;
    }

  });

}());
