/* jshint browser: true */
/* jshint unused: false */
/* global frontendConfig, Backbone, templateEngine, $, window, noty, arangoHelper, Noty */
(function () {
  'use strict';

  window.NotificationView = Backbone.View.extend({
    events: {
      'click .navlogo #stat_hd': 'toggleNotification',
      'click .notificationItem .fa': 'removeNotification',
      'click #removeAllNotifications': 'removeAllNotifications'
    },

    initialize: function () {
      this.collection.bind('add', this.renderNotifications.bind(this));
      this.collection.bind('remove', this.renderNotifications.bind(this));
      this.collection.bind('reset', this.renderNotifications.bind(this));

      window.setTimeout(function () {
        if (frontendConfig.authenticationEnabled === false && frontendConfig.isCluster === false && arangoHelper.showAuthDialog() === true) {
          window.arangoHelper.arangoWarning(
            'Warning', 'Authentication is disabled. Do not use this setup in production mode.'
          );
        }
      }, 2000);
    },

    notificationItem: templateEngine.createTemplate('notificationItem.ejs'),

    el: '#notificationBar',

    template: templateEngine.createTemplate('notificationView.ejs'),

    toggleNotification: function () {
      var counter = this.collection.length;
      if (counter !== 0) {
        $('#notification_menu').toggle();
      }
    },

    removeAllNotifications: function () {
      Noty.clearQueue();
      Noty.closeAll();
      this.collection.reset();
      $('#notification_menu').hide();
    },

    removeNotification: function (e) {
      var cid = e.target.id;
      this.collection.get(cid).destroy();
    },

    renderNotifications: function (a, b, event) {
      if (event) {
        if (event.add) {
          var latestModel = this.collection.at(this.collection.length - 1);
          var message = latestModel.get('title');
          var time = 5000;
          var closeWidth = ['click'];
          var buttons;

          if (latestModel.get('content')) {
            message = message + ': ' + latestModel.get('content');
          }

          if (latestModel.get('type') === 'error') {
            time = false;
            closeWidth = ['button'];

            buttons = [
              Noty.button('Close', 'btn btn-error', function (n) {
                console.log('button 1 clicked');
                n.close();
              }, {id: 'button1', 'data-status': 'ok'})
            ];
          } else if (latestModel.get('type') === 'warning') {
            time = 15000;

            buttons = [
              Noty.button('Close', 'btn btn-warning', function (n) {
                n.close();
              }, {id: 'button2', 'data-status': 'true'}),

              Noty.button('Do not show again.', 'btn btn-error', function (n) {
                window.arangoHelper.doNotShowAgain();
                n.close();
              }, {id: 'button3', 'data-status': 'false'})
            ];
          } else if (latestModel.get('type') === 'info') {
            time = 10000;
          }

          arangoHelper.hideArangoNotifications();

          new Noty({
            type: latestModel.get('type'),
            layout: 'bottom',
            theme: 'sunset',
            text: message,
            buttons: buttons,
            timeout: time,
            closeWidth: closeWidth
          }).show();

          if (latestModel.get('type') === 'success') {
            latestModel.destroy();
            return;
          }
        }
      }
    },

    render: function () {
      $(this.el).html(this.template.render({
        notifications: this.collection
      }));

      this.renderNotifications();
      this.delegateEvents();

      return this.el;
    }

  });
}());
