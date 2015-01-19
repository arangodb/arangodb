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
      window.App.navigate('applications/' + encodeURIComponent(this.model.get('_key')), { trigger: true });
    },

    toggle: function(type, shouldShow) {
      switch (type) {
        case "devel":
          if (this.model.get("development") === true) {
            this._show = shouldShow;
          }
          break;
        case "production":
          if (this.model.get("development") === false && this.model.get("isSystem") === false) {
            this._show = shouldShow;
          }
          break;
        case "system":
          if (this.model.get("isSystem") === true) {
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
      $(this.el).html(this.template.render(this.model));
      return $(this.el);
    }
  });
}());
