/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  window.ProgressView = Backbone.View.extend({

    template: templateEngine.createTemplate("progressBase.ejs"),

    el: "#progressPlaceholder",

    el2: "#progressPlaceholderIcon",

    action: function(){},

    events: {
      "click .progress-action button": "performAction"
    },

    performAction: function() {
      this.action();
    },

    initialize: function() {
    },

    show: function(msg, action, button) {
      $(this.el).html(this.template.render({}));
      $(".progress-text").text(msg);
      $(".progress-action").html(button);

      this.action = action;

      $(this.el).show();
      //$(this.el2).html('<i class="fa fa-spinner fa-spin"></i>');
    },

    hide: function() {
      $(this.el).hide();
      this.action = function(){};
      //$(this.el2).html('');
    }

  });
}());
