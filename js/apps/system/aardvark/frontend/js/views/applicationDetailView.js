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
    return $(this.el);
  },

  openApp: function() {
    alert('Open!');
  },

  deleteApp: function() {
    alert('Delete!');
  }
});
