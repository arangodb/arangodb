/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  window.ProgressView = Backbone.View.extend({

    template: templateEngine.createTemplate("progressBase.ejs"),
    el: "#progressPlaceholder",

    initialize: function() {
    },

    show: function(msg) {
      $(this.el).html(this.template.render({}));
      $(".progress-message").text(msg);
      $(this.el).fadeIn();
    },

    hide: function() {
      $(this.el).fadeOut();
    }

  });
}());
