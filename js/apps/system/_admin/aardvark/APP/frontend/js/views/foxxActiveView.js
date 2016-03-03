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

      var conf = function() {
        if (this.model.needsConfiguration()) {

          if ($(this.el).find('.warning-icons').length > 0) {
            $(this.el).find('.warning-icons')
            .append('<span class="fa fa-cog" title="Needs configuration"></span>');
          }
          else {
            $(this.el).find('img')
            .after(
              '<span class="warning-icons"><span class="fa fa-cog" title="Needs configuration"></span></span>'
            );
          }
        }
      }.bind(this);

      var depend = function() {
        if (this.model.hasUnconfiguredDependencies()) {

          if ($(this.el).find('.warning-icons').length > 0) {
            $(this.el).find('.warning-icons')
            .append('<span class="fa fa-cubes" title="Unconfigured dependencies"></span>');
          }
          else {
            $(this.el).find('img')
            .after(
              '<span class="warning-icons"><span class="fa fa-cubes" title="Unconfigured dependencies"></span></span>'
            );
          }
        }
      }.bind(this);

      /*isBroken function in model doesnt make sense
      var broken = function() {
        $(this.el).find('warning-icons')
        .append('<span class="fa fa-warning" title="Mount error"></span>');
      }.bind(this);
       */

      this.model.getConfiguration(conf);
      this.model.getDependencies(depend);

      return $(this.el);
    }
  });
}());
