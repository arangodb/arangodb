/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, plannerTemplateEngine */
(function() {

    "use strict";

    window.PlanScenarioSelectorView = Backbone.View.extend({
        el: '#content',

        template: plannerTemplateEngine.createTemplate("planScenarioSelector.ejs", "planner"),


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
        }
    });
}());
