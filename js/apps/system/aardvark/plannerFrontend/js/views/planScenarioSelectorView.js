/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine, GraphViewerUI */
/*global require*/
(function() {

    "use strict";

    window.PlanScenarioSelectorView = Backbone.View.extend({
        el: '#content',

        template: plannerTemplateEngine.createTemplate("planScenarioSelector.ejs", "planner"),


        events: {
        /*    "click input[type='radio'][name='loadtype']": "toggleLoadtypeDisplay",
            "click #createViewer": "createViewer",
            "click #add_label": "insertNewLabelLine",
            "click #add_colour": "insertNewColourLine",
            "click #add_group_by": "insertNewGroupLine",
            "click input[type='radio'][name='colour']": "toggleColourDisplay",
            "click .gv_internal_remove_line": "removeAttrLine",
            "click #manageGraphs": "showGraphManager"*/
        },

        showGraphManager: function() {
            window.App.navigate("graphManagement", {trigger: true});
        },

/*
        removeAttrLine: function(e) {
            var g = $(e.currentTarget)
                    .parent()
                    .parent(),
                set = g.parent();
            set.get(0).removeChild(g.get(0));
        },
*/



        render: function() {
            $(this.el).html(this.template.render({}));
        }

    });
}());
