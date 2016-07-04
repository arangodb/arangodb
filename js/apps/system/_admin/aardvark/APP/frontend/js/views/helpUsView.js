/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window*/
(function () {
  'use strict';

  window.HelpUsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('helpUsView.ejs'),

    render: function () {
      this.$el.html(this.template.render({}));
    }

  });
}());
