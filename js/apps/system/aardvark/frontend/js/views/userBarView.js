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
      "click #userLogout"             : "userLogout"
    },

    initialize: function () {
      this.userCollection = this.options.userCollection;
      this.userCollection.fetch({async:false});
      this.userCollection.bind("change:extra", this.render.bind(this));
    },

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

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "user") {
        $("#user_dropdown").show(200);
        return;
      }
    },

    hideDropdown: function (e) {
      $("#user_dropdown").hide();
    },

    render: function (el) {
      var username = this.userCollection.whoAmI(),
        img = null,
        name = null,
        active = false,
        currentUser = null;
      if (username !== null) {
        currentUser = this.userCollection.findWhere({user: username});
        currentUser.set({loggedIn : true});
        name = currentUser.get("extra").name;
        img = currentUser.get("extra").img;
        active = currentUser.get("active");
      }
      if (!img) {
        img = "img/arangodblogoAvatar_24.png";
      } else {
        img = "https://s.gravatar.com/avatar/" + img + "?s=24";
      }
      if (!name) {
        name = "";
      }

      this.$el = $("#userBar");
      this.$el.html(this.template.render({
        img : img,
        name : name,
        username : username,
        active : active
      }));

      this.delegateEvents();
      return this.$el;
    },

    userLogout : function() {
      this.userCollection.whoAmI();
      this.userCollection.logout();
    }
  });
}());
