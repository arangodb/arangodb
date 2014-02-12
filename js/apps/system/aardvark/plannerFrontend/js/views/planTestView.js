/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine */

(function() {
  "use strict";
  
  window.PlanTestView = Backbone.View.extend({
    el: "#content",
    template: plannerTemplateEngine.createTemplate("testPlan.ejs", "planner"),

    events: {
      "click #startPlan": "startPlan"
    },

    render: function() {
      $(this.el).html(this.template.render({}));
    }
  });

}());
