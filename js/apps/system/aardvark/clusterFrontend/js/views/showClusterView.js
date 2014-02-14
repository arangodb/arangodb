/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ShowClusterView = Backbone.View.extend({

    el: "#content",

    template: clusterTemplateEngine.createTemplate("showCluster.ejs"),

    initialize: function() {
        this.dbservers = new window.ClusterServers();
        this.dbservers.fetch({
            async : false
        });
        this.coordinators = new window.ClusterCoordinators();
        this.coordinators.fetch({
            async : false
        });
        this.data = [];
        if (this.statisticsDescription === undefined) {
            this.statisticsDescription = new window.StatisticsDescription();
            this.statisticsDescription.fetch({
                async:false
            });
        }
        var self = this;
        this.dbservers.forEach(function (server) {
            var statCollect  = new window.StatisticsCollection();
            statCollect.url = server.get("address").replace("tcp", "http") + "/_admin/statistics";
            self.data.push(statCollect);
            statCollect.fetch();
        });

    },

    render: function() {
      console.log(this.data);
      console.log(this.coordinators.getOverview());
      $(this.el).html(this.template.render({}));
    }
  });

}());
