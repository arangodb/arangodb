/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, templateEngine, $, window */

(function() {
  "use strict";

  window.ClusterDashboardView = Backbone.View.extend({
    
    el: '#content',

    template: templateEngine.createTemplate("clusterDashboardView.ejs"),

    initialize: function() {
      this.dbservers = new window.ClusterServers();
      this.coordinators = new window.ClusterCoordinators();
    },

    render: function(){
      $(this.el).html(this.template.render({}));
      this.overView = new window.ClusterOverviewView({
        dbservers: this.dbservers,
        coordinators: this.coordinators
      });
      this.overView.render();
      return this;
    }

  });

}());
