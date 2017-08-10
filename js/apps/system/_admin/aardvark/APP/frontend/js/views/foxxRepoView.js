/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, window, templateEngine */

(function () {
  'use strict';

  window.FoxxRepoView = Backbone.View.extend({
    tagName: 'div',
    className: 'foxxTile tile pure-u-1-1 pure-u-sm-1-2 pure-u-md-1-3 pure-u-lg-1-4 pure-u-xl-1-5',
    template: templateEngine.createTemplate('foxxRepoView.ejs'),
    _show: true,

    events: {
      'click': 'openAppDetailView'
    },

    openAppDetailView: function () {
      window.App.navigate('service/' + encodeURIComponent(this.model.get('mount')), { trigger: true });
    },

    render: function () {
      this.model.fetchThumbnail(function () {
        $(this.el).html(this.template.render({
          model: this.model.toJSON()
        }));
      }.bind(this));

      return $(this.el);
    }
  });
}());
