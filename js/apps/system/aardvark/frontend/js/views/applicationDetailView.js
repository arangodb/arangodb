/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _*/

window.ApplicationDetailView = Backbone.View.extend({
  el: '#content',

  template: templateEngine.createTemplate('applicationDetailView.ejs'),

  events: {
    'click .open': 'openApp',
    'click .delete': 'deleteApp'
  },

  render: function() {
    $(this.el).html(this.template.render(this.model));

    $.get(this.appUrl()).success(function () {
      $('.open', this.el).prop('disabled', false);
    }, this);

    return $(this.el);
  },

  openApp: function() {
    window.open(this.appUrl(), this.model.get('title')).focus();
  },

  deleteApp: function() {
    alert('Delete!');
  },

  appUrl: function () {
    return window.location.origin + '/_db/' +
      encodeURIComponent(arangoHelper.currentDatabase()) +
      this.model.get('mount');
  },
});
