/*jshint browser: true */
/*jshint unused: false */
/*global frontendConfig, Backbone, templateEngine, $, window, noty */
(function () {
  "use strict";

  window.NotificationView = Backbone.View.extend({

    events: {
      "click .navlogo #stat_hd"       : "toggleNotification",
      "click .notificationItem .fa"   : "removeNotification",
      "click #removeAllNotifications" : "removeAllNotifications"
    },

    initialize: function () {
      this.collection.bind("add", this.renderNotifications.bind(this));
      this.collection.bind("remove", this.renderNotifications.bind(this));
      this.collection.bind("reset", this.renderNotifications.bind(this));

      // TODO save user property if check should be enabled/disabled
      window.setTimeout(function() {
        if (frontendConfig.authenticationEnabled === false) {
          window.arangoHelper.arangoWarning(
            "Warning", "Authentication is disabled. Do not use this setup in production mode."
          );
        }
      }, 2000);
    },

    notificationItem: templateEngine.createTemplate("notificationItem.ejs"),

    el: '#notificationBar',

    template: templateEngine.createTemplate("notificationView.ejs"),

    toggleNotification: function () {
      var counter = this.collection.length;
      if (counter !== 0) {
        $('#notification_menu').toggle();
      }
    },

    removeAllNotifications: function () {
      $.noty.clearQueue();
      $.noty.closeAll();
      this.collection.reset();
      $('#notification_menu').hide();
    },

    removeNotification: function(e) {
      var cid = e.target.id;
      this.collection.get(cid).destroy();
    },

    renderNotifications: function(a, b, event) {

      if (event) {
        if (event.add) {
          var latestModel = this.collection.at(this.collection.length - 1),
          message = latestModel.get('title'),
          time = 3000,
          closeWidth = ['click'],
          buttons;

          if (latestModel.get('content')) {
            message = message + ": " + latestModel.get('content');
          }

          if (latestModel.get('type') === 'error') {
            time = false;
            closeWidth = ['button'];
            buttons = [{
              addClass: 'button-danger', text: 'Close', onClick: function($noty) {
                $noty.close();
              }
            }];
          }
          else if (latestModel.get('type') === 'warning') {
            time = 20000;
          }

          $.noty.clearQueue();
          $.noty.closeAll();

          noty({
            theme: 'relax',
            text: message,
            template: 
              '<div class="noty_message arango_message">' + 
              '<div><i class="fa fa-close"></i></div><span class="noty_text arango_text"></span>' + 
              '<div class="noty_close arango_close"></div></div>',
            maxVisible: 1,
            closeWith: ['click'],
            type: latestModel.get('type'),
            layout: 'bottom',
            timeout: time,
            buttons: buttons,
            animation: {
              open: {height: 'show'},
              close: {height: 'hide'},
              easing: 'swing',
              speed: 200,
              closeWith: closeWidth
            }
          });

          if (latestModel.get('type') === 'success') {
            latestModel.destroy();
            return;
          }
        }
      }

      $('#stat_hd_counter').text(this.collection.length);
      if (this.collection.length === 0) {
        $('#stat_hd').removeClass('fullNotification');
        $('#notification_menu').hide();
      }
      else {
        $('#stat_hd').addClass('fullNotification');
      }

      $('.innerDropdownInnerUL').html(this.notificationItem.render({
        notifications : this.collection
      }));
      $('.notificationInfoIcon').tooltip({
        position: {
          my: "left top",
          at: "right+55 top-1"
        }
      });
    },

    render: function () {
      $(this.el).html(this.template.render({
        notifications : this.collection
      }));

      this.renderNotifications();
      this.delegateEvents();

      return this.el;
    }

  });
}());
