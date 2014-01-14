/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, window, $ */

(function() {
  "use strict";

  window.ClusterOverviewView = Backbone.View.extend({
    
    el: '#clusterOverview',

    template: templateEngine.createTemplate("clusterOverviewView.ejs"),

    events: {
      "click #primary": "loadPrimaries",
      "click #secondary": "loadSecondaries",
      "click #coordinator": "loadCoordinators"
    },

    initialize: function() {
      this.serverView = new window.ClusterServerView();
    },

    loadPrimaries: function() {
      this.serverView.render({
        type: "primary"
      });
    },

    loadSecondaries: function() {
      this.serverView.render({
        type: "secondary"
      });
    },

    loadCoordinators: function() {
      this.serverView.render({
        type: "coordinator"
      });
    },

    render: function(info){
      $(this.el).html(this.template.render({}));
      return this;
    }

  });

}());
