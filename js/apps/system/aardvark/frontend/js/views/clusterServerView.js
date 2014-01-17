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

      this.fakeData = [
        {
          primary: {
            name: "Pavel",
            url: "tcp://192.168.0.1:1337",
            status: "ok"
          },
          secondary: {
            name: "Sally",
            url: "tcp://192.168.1.1:1337",
            status: "ok"
          }
        },
        {
          primary: {
            name: "Pancho",
            url: "tcp://192.168.0.2:1337",
            status: "ok"
          }
        },
        {
          primary: {
            name: "Pablo",
            url: "tcp://192.168.0.5:1337",
            status: "critical"
          },
          secondary: {
            name: "Sandy",
            url: "tcp://192.168.1.5:1337",
            status: "critical"
          }
        }
      ];
    },

    loadServer: function(e) {
      var id = e.currentTarget.id;
      this.dbView.render({
        name: id
      });
      this.render(true);
    },

    unrender: function() {
      $(this.el).html("");
      this.dbView.unrender();
    },

    render: function(minify){
      if(!minify) {
        this.dbView.unrender();
      }
      $(this.el).html(this.template.render({
        minify: minify,
        servers: this.fakeData
      }));
      return this;
    }

  });

}());
