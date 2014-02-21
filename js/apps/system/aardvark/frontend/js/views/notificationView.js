/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, window*/
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
    },

    notificationItem: templateEngine.createTemplate("notificationItem.ejs"),

    el: '#notificationBar',

    template: templateEngine.createTemplate("notificationView.ejs"),

    toggleNotification: function (e) {
      var counter = this.collection.length;
      if (counter !== 0) {
        $('#notification_menu').toggle();
      }
    },

    removeAllNotifications: function () {
      this.collection.reset();
      $('#notification_menu').hide();
    },

    removeNotification: function(e) {
      var cid = e.target.id;
      this.collection.get(cid).destroy();
    },

    renderNotifications: function() {
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
