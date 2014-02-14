/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ShowClusterView = Backbone.View.extend({

    el: "#content",

    template: clusterTemplateEngine.createTemplate("showCluster.ejs"),

    events: {
      "click #startPlan": "startPlan"
    },

    render: function() {
      $(this.el).html(this.template.render({}));
    }
  });

}());
