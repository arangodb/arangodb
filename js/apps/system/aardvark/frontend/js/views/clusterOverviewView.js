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
      this.dbservers = this.options.dbservers;
      this.coordinators = this.options.coordinators;
      this.serverView = new window.ClusterServerView({
        collection: this.dbservers
      });
      this.coordinatorView = new window.ClusterCoordinatorView({
        collection: this.coordinators
      });
    },

    loadDBServers: function() {
      this.coordinatorView.unrender();
      this.serverView.render();
      this.render(true);
    },

    loadCoordinators: function() {
      this.serverView.unrender();
      this.coordinatorView.render();
      this.render(true);
    },

    render: function(minify){
      $(this.el).html(this.template.render({
        minify: minify,
        dbservers: this.dbservers.getOverview(),
        coordinators: this.coordinators.getOverview()
      }));
      $(this.el).toggleClass("clusterColumnMax", !minify);
      return this;
    }

  });

}());
