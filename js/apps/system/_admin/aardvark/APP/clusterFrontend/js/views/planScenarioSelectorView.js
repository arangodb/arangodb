/*global Backbone, $, _, window, templateEngine */
(function() {

  "use strict";

  window.PlanScenarioSelectorView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("planScenarioSelector.ejs", "planner"),


    events: {
      "click #multiServerAsymmetrical": "multiServerAsymmetrical",
      "click #singleServer": "singleServer"
    },

    render: function() {
      $(this.el).html(this.template.render({}));
    },

    multiServerAsymmetrical: function() {
      window.App.navigate(
        "planAsymmetrical", {trigger: true}
      );
    },

    singleServer: function() {
      window.App.navigate(
        "planTest", {trigger: true}
      );
    }

  });
}());
