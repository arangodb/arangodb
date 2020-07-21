/* jshint browser: true */
/* jshint unused: false */
/* global _, Backbone, templateEngine, $, window */
(function () {
  'use strict';

  window.SupportView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('supportView.ejs'),

    events: {
      'click .subViewNavbar .subMenuEntry': 'toggleViews'
    },

    render: function () {
      this.$el.html(this.template.render({
        parsedVersion: window.versionHelper.toDocuVersion(
          window.frontendConfig.version.version
        )
      }));
    },

    resize: function (auto) {
      if (auto) {
        $('.innerContent').css('height', 'auto');
      } else {
        $('.innerContent').height($('.centralRow').height() - 170);
      }
    },

    renderSwagger: function () {
      var path = window.location.pathname.split('/');
      var url = window.location.protocol + '//' + window.location.hostname + ':' + window.location.port + '/' + path[1] + '/' + path[2] + '/_admin/aardvark/api/index.html?collapsed';
      $('#swagger').html('');
      $('#swagger').append(
        '<iframe src="' + url + '" style="border:none"></iframe>'
      );
    },

    toggleViews: function (e) {
      var self = this;
      var id = e.currentTarget.id.split('-')[0];
      var views = ['community', 'documentation', 'swagger'];

      _.each(views, function (view) {
        if (id !== view) {
          $('#' + view).hide();
        } else {
          if (id === 'swagger') {
            self.renderSwagger();
            $('#swagger iframe').css('height', '100%');
            $('#swagger iframe').css('width', '100%');
            $('#swagger iframe').css('margin-top', '-13px');
            self.resize();
          } else {
            self.resize(true);
          }
          $('#' + view).show();
        }
      });

      $('.subMenuEntries').children().removeClass('active');
      $('#' + id + '-support').addClass('active');
    }

  });
}());
