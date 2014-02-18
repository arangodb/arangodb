/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";

  window.UserBarView = Backbone.View.extend({

    events: {
      "change #userBarSelect": "navigateBySelect",
      "click .tab": "navigateByTab",
      "mouseenter .dropdown": "showDropdown",
      "mouseleave .dropdown": "hideDropdown",
      "click .navlogo #stat_hd" : "toggleNotification"
    },

    initialize: function () {
      this.notificationList = new window.NotificationCollection();
    },

    notificationItem: templateEngine.createTemplate("notificationItem.ejs"),

    template: templateEngine.createTemplate("userBarView.ejs"),

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).closest("a");
      var navigateTo = tab.attr("id");
      if (navigateTo === "user") {
        $("#user_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      window.App.navigate(navigateTo, {trigger: true});
      e.preventDefault();
    },

    toggleNotification: function (e) {
      $('#notification_menu').toggle();
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "user") {
        $("#user_dropdown").show(200);
        return;
      }
    },

    hideDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "") {
        tab = $(tab).closest(".user-dropdown-menu");
        navigateTo = tab.attr("id");
      }
      if (navigateTo === "user" || navigateTo === "user_dropdown" || navigateTo === "userimage" ) {
        $("#user_dropdown").hide();
        return;
      }
    },

    updateNotifications: function() {
      this.renderNotifications();
    },

    renderNotifications: function() {
      $('.innerDropdownInnerUL').html(this.notificationItem.render({
        notifications : this.notificationList.models
      }));
    },

    render: function (el) {
      this.$el = el;
      this.$el.html(this.template.render({
        img : "https://s.gravatar.com/avatar/9c53a795affc3c3c03801ffae90e2e11?s=80",
        prename : "Floyd",
        lastname : "Pepper",
        notifications : this.notificationList.models
      }));

      this.renderNotifications();

      this.delegateEvents();
      this.updateNotifications();
      return this.$el;
    }
  });
}());
