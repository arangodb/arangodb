/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, window, $ */

(function() {
  "use strict";

  window.ClusterOverviewView = Backbone.View.extend({
    
    el: '#clusterOverview',

    template: templateEngine.createTemplate("clusterOverviewView.ejs"),

    events: {
      "click #dbserver": "loadDBServers",
      "click #coordinator": "loadCoordinators"
    },

    initialize: function() {
      this.fakeData = {
          dbservers: {
            plan: 3,
            having: 3,
            status: "warning"
          },
          coordinators: {
            plan: 3,
            having: 2,
            status: "critical"
          }
        };

      this.serverView = new window.ClusterServerView();
    },

    loadDBServers: function() {
      this.serverView.render({
        type: "dbservers"
      });
      this.render(true);
    },

    loadCoordinators: function() {
      this.serverView.render({
        type: "coordinator"
      });
      this.render(true);
    },

    render: function(minify){
      $(this.el).html(this.template.render({
        minify: minify,
        dbservers: this.fakeData.dbservers,
        coordinators: this.fakeData.coordinators
      }));
      $(this.el).toggleClass("clusterColumnMax", !minify);
      return this;
    }

  });

}());
