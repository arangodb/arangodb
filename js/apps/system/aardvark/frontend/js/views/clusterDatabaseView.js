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
      this.colView = new window.ClusterCollectionView();
      this.fakeData = {
        databases: [
          {
            name: "_system",
            status: "ok"
          },
          {
            name: "myDatabase",
            status: "warning"
          },
          {
            name: "otherDatabase",
            status: "critical"
          }
        ]
      };
    },

    loadDatabase: function(e) {
      var id = e.currentTarget.id;
      this.colView.render(id);
    },

    unrender: function() {
      $(this.el).html("");
      this.colView.unrender();
    },

    render: function(){
      $(this.el).html(this.template.render({
        databases: this.fakeData.databases
      }));
      this.colView.unrender();
      return this;
    }

  });

}());
