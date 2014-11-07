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

    toShow: false,

    action: function(){},

    events: {
      "click .progress-action button": "performAction",
    },

    performAction: function() {
      //this.action();
      window.progressView.hide();
    },

    initialize: function() {
    },

    showWithDelay: function(delay, msg, action, button) {
    var self = this;
      self.toShow = true;

      setTimeout(function(delay) {
        if (self.toShow === true) {
          self.show(msg, action, button);
        }
      }, delay);
    },

    show: function(msg, action, button) {
      $(this.el).html(this.template.render({}));
      $(".progress-text").text(msg);
      $(".progress-action").html('<button class="button-danger">Cancel</button>');
      this.action = this.hide();
      //$(".progress-action").html(button);
      //this.action = action;

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
