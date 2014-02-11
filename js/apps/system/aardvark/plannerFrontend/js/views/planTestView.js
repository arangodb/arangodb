/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, document, arangoCollection,arangoHelper,dashboardView,arangoDatabase*/

(function() {
  "use strict";
  
  window.PlanTestView = Backbone.View.extend({
    el: "#content",
    template: plannerTemplateEngine.createTemplate("testPlan.ejs", "planner"),

    render: function() {
      console.log("Rendoror");
      $(this.el).html(this.template.render({}));
    }
  });

}());
