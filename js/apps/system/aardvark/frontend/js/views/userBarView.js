/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";

  window.UserBarView = Backbone.View.extend({

    events: {
      "change #userBarSelect"         : "navigateBySelect",
      "click .tab"                    : "navigateByTab",
      "mouseenter .dropdown"          : "showDropdown",
      "mouseleave .dropdown"          : "hideDropdown",
      "click .navlogo #stat_hd"       : "toggleNotification",
      "click .notificationItem .fa"   : "removeNotification",
      "click #removeAllNotifications" : "removeAllNotifications",
      "click #userLogout"             : "userLogout"
    },

    initialize: function () {
      this.collection.bind("add", this.renderNotifications.bind(this));
      this.collection.bind("remove", this.renderNotifications.bind(this));
      this.collection.bind("reset", this.renderNotifications.bind(this));
      this.userCollection = this.options.userCollection;
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

    removeAllNotifications: function () {
      this.collection.reset();
    },

    removeNotification: function(e) {
      var cid = e.target.id;
      this.collection.get(cid).destroy();
    },

    renderNotifications: function() {

      $('#stat_hd_counter').text(this.collection.length);
      if (this.collection.length === 0) {
        $('#stat_hd').removeClass('fullNotification');
      }
      else {
        $('#stat_hd').addClass('fullNotification');
      }

      $('.innerDropdownInnerUL').html(this.notificationItem.render({
        notifications : this.collection
      }));
    },

    render: function (el) {
      var username = this.userCollection.whoAmI(),
        img = null,
        name = null,
        active = false,
        currentUser = null;
      if (username !== null) {
        this.userCollection.fetch({async:false});
        currentUser = this.userCollection.findWhere({user: username});
        currentUser.set({loggedIn : true});
        name = currentUser.get("extra").name;
        img = currentUser.get("extra").img;
        active = currentUser.get("active");
      }
      if (!img) {
        img = "img/arangodblogoAvatar.png";
      } else {
        img = "https://s.gravatar.com/avatar/" + img + "?s=23";
      }
      if (!name) {
        name = "";
      }

      this.$el = el;
      this.$el.html(this.template.render({
        img : img,
        name : name,
        username : username,
        active : active,
        notifications : this.collection
      }));

      this.renderNotifications();

      this.delegateEvents();
      this.renderNotifications();
      return this.$el;
    },

    userLogout : function() {
      this.userCollection.whoAmI();
      this.userCollection.logout();
    }
  });
}());
