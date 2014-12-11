/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, EJS, arangoHelper, _, templateEngine, Joi*/

(function() {
  'use strict';

  window.FoxxActiveView = Backbone.View.extend({
    tagName: 'div',
    className: 'tile',
    template: templateEngine.createTemplate('foxxActiveView.ejs'),

    events: {
      'click' : 'openAppDetailView'
    },

    openAppDetailView: function() {
      window.App.navigate('applications/' + encodeURIComponent(this.model.get('_key')), { trigger: true });
    },

    render: function(){
      $(this.el).html(this.template.render(this.model));
      return $(this.el);
    }
  });
}());
