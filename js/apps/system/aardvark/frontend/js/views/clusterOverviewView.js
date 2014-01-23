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
      this.isMinified = false;
      this.dbservers = this.options.dbservers;
      this.coordinators = this.options.coordinators;
      this.serverView = new window.ClusterServerView({
        collection: this.dbservers
      });
      this.coordinatorView = new window.ClusterCoordinatorView({
        collection: this.coordinators
      });
      var rerender = function() {
        this.render(this.isMinified);
        if (this.selectedSub === "server") {
          this.serverView.render(this.serverView.isMinified);
        }
        if (this.selectedSub === "coordinators") {
          this.coordinatorView.render();
        }
      };
      this.selectedSub = null;
      this.dbservers.bind("add", rerender.bind(this));
      this.dbservers.bind("change", rerender.bind(this));
      this.dbservers.bind("remove", rerender.bind(this));
      this.coordinators.bind("add", rerender.bind(this));
      this.coordinators.bind("change", rerender.bind(this));
      this.coordinators.bind("remove", rerender.bind(this));
    },

    loadDBServers: function() {
      this.selectedSub = "server";
      this.coordinatorView.unrender();
      this.serverView.render();
      this.render(true);
    },

    loadCoordinators: function() {
      this.selectedSub = "coordinators";
      this.serverView.unrender();
      this.coordinatorView.render();
      this.render(true);
    },

    stopUpdating: function() {
      this.dbservers.stopUpdating();
      this.coordinators.stopUpdating();
      this.serverView.stopUpdating();
    },

    render: function(minify){
      this.dbservers.startUpdating();
      this.coordinators.startUpdating();
      this.isMinified = !!minify;
      if (!minify) {
        this.selectedSub = null;
      }
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
