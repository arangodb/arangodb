/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _*/

window.ApplicationDetailView = Backbone.View.extend({
  el: '#content',

  template: templateEngine.createTemplate('applicationDetailView.ejs'),

  events: {
  },

  render: function() {
    console.log(this.model);
    $(this.el).html(this.template.render(this.model));
    return $(this.el);
  }
});
