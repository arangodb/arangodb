/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, EJS, arangoHelper, _, templateEngine, Joi*/

(function() {
  'use strict';

  window.FoxxActiveView = Backbone.View.extend({
    tagName: 'div',
    className: 'tile',
    template: templateEngine.createTemplate('foxxActiveView.ejs'),
    _show: true,

    events: {
      'click' : 'openAppDetailView'
    },

    openAppDetailView: function() {
      window.App.navigate('applications/' + encodeURIComponent(this.model.get('mount')), { trigger: true });
    },

    toggle: function(type, shouldShow) {
      switch (type) {
        case "devel":
          if (this.model.isDevelopment()) {
            this._show = shouldShow;
          }
          break;
        case "production":
          if (!this.model.isDevelopment() && !this.model.isSystem()) {
            this._show = shouldShow;
          }
          break;
        case "system":
          if (this.model.isSystem()) {
            this._show = shouldShow;
          }
          break;
        default:
      }
      if (this._show) {
        $(this.el).show();
      } else {
        $(this.el).hide();
      }
    },

    render: function(){
      $(this.el).html(this.template.render({
        model: this.model
      }));
      return $(this.el);
    }
  });
}());
