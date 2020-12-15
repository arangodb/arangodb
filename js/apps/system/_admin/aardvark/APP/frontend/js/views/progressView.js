/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, window, setTimeout */
/* global templateEngine */

(function () {
  'use strict';

  window.ProgressView = Backbone.View.extend({
    template: templateEngine.createTemplate('progressBase.ejs'),

    el: '#progressPlaceholder',

    el2: '#progressPlaceholderIcon',

    toShow: false,
    lastDelay: 0,

    action: function () {},

    events: {
      'click .progress-action button': 'performAction'
    },

    performAction: function () {
      if (typeof this.action === 'function') {
        this.action();
      }
      window.progressView.hide();
    },

    initialize: function () {},

    showWithDelay: function (delay, msg, action, button) {
      var self = this;
      self.toShow = true;
      self.lastDelay = delay;

      setTimeout(function () {
        if (self.toShow === true) {
          self.show(msg, action, button);
        }
      }, self.lastDelay);
    },

    show: function (msg, callback, buttonText) {
      $(this.el).html(this.template.render({}));
      $('.progress-text').text(msg);

      if (!buttonText) {
        $('.progress-action').html('<button class="button-danger">Cancel</button>');
      } else {
        $('.progress-action').html('<button class="button-danger">' + buttonText + '</button>');
      }

      if (!callback) {
        this.action = this.hide();
      } else {
        this.action = callback;
      }
      // $(".progress-action").html(button)
      // this.action = action

      $(this.el).show();
    // $(this.el2).html('<i class="fa fa-spinner fa-spin"></i>')
    },

    hide: function () {
      var self = this;
      self.toShow = false;

      $(this.el).hide();

      this.action = function () {};
    }

  });
}());
