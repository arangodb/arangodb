/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterCoordinatorView = Backbone.View.extend({
    
    el: '#clusterServers',

    template: templateEngine.createTemplate("clusterCoordinatorView.ejs"),

    initialize: function() {
      this.fakeData = {
        coordinators: [
          {
            name: "Charly",
            url: "tcp://192.168.0.1:1337",
            status: "ok"
          },
          {
            name: "Carlos",
            url: "tcp://192.168.0.2:1337",
            status: "critical"
          },
          {
            name: "Chantalle",
            url: "tcp://192.168.0.5:1337",
            status: "ok"
          }
        ]
      };
    },

    unrender: function() {
      $(this.el).html("");
    },

    render: function() {
      $(this.el).html(this.template.render({
        coordinators: this.fakeData.coordinators
      }));
      return this;
    }

  });

}());
