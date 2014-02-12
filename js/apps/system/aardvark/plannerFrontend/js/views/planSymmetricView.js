/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine */

(function() {
  "use strict";
  
  window.PlanSymmetricView = Backbone.View.extend({
    el: "#content",
    template: plannerTemplateEngine.createTemplate("symmetricPlan.ejs"),

    events: {
      "click #startPlan": "startPlan"
    },

    render: function() {
      $(this.el).html(this.template.render({}));
    }
  });

}());
