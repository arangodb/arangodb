/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, templateEngine, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ClusterUnreachableView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterUnreachable.ejs"),

    events: {
    },

    initialize: function() {
      this.coordinators = new window.ClusterCoordinators([], {
      });
    },


    retryConnection: function() {
      this.coordinators.checkConnection(function() {
        window.App.showCluster();
      });
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      window.setTimeout(this.retryConnection.bind(this), 10000);
    }

  });
}());
