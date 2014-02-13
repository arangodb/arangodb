/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, window*/
(function () {
    "use strict";

    window.UserBarView = Backbone.View.extend({
        el: '#statisticBar',

        events: {

        },

        template: templateEngine.createTemplate("userBarView.ejs"),

        render: function() {
            $(this.el).html(this.template.render(this.template.text));
            return this;

        }
    });
}());