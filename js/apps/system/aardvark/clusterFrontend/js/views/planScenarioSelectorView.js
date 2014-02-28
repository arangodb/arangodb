/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, plannerTemplateEngine */
(function() {

    "use strict";

    window.PlanScenarioSelectorView = Backbone.View.extend({
        el: '#content',

        template: templateEngine.createTemplate("planScenarioSelector.ejs", "planner"),


        events: {
            "click #multiServerSymmetrical": "multiServerSymmetrical",
            "click #multiServerAsymmetrical": "multiServerAsymmetrical",
            "click #singleServer": "singleServer"
        },

        render: function() {
          $(this.el).html(this.template.render({}));
        },
        multiServerSymmetrical: function() {
          window.App.navigate(
            "planSymmetrical", {trigger: true}
          );
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
        },

        readJSON: function(fileInput, callback) {
          var file = fileInput.files[0];
          var textType = 'application/json';
          if (file.type.match(textType)) {
            var reader = new FileReader();
            reader.onload = callback.bind(reader);
            reader.readAsText(file);
          }
        }

    });
}());
