/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";
  window.NavigationView = Backbone.View.extend({
    el: '#navigationBar',

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab",
      "click .internalLink": "navigateByTab",
      "mouseenter .dropdown": "showDropdown",
      "mouseleave .dropdown": "hideDropdown"
    },

    initialize: function () {
      this.dbSelectionView = new window.DBSelectionView({
        collection: window.arangoDatabase,
        current: window.currentDB
      });
      this.userBarView = new window.UserBarView({
        collection: this.options.notificationCollection,
        userCollection: window.userCollection
      });
      this.statisticBarView = new window.StatisticBarView({});
    },

    handleSelectDatabase: function () {
      this.dbSelectionView.render($("#dbSelect"));
    },


    template: templateEngine.createTemplate("navigationView.ejs"),

    render: function () {
      $(this.el).html(this.template.render({
        isSystem: window.currentDB.get("isSystem")
      }));
      this.dbSelectionView.render($("#dbSelect"));
      this.userBarView.render($("#userBar"));
      this.statisticBarView.render($("#statisticBar"));
      return this;
    },

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "links") {
        e.preventDefault();
        return;
      }
      if (navigateTo === "tools") {
        e.preventDefault();
        return;
      }
      if (navigateTo === "dbselection") {
        e.preventDefault();
        return;
      }
      window.App.navigate(navigateTo, {trigger: true});
      e.preventDefault();
    },

    handleSelectNavigation: function () {
      $("#arangoCollectionSelect").change(function () {
        var navigateTo = $(this).find("option:selected").val();
        window.App.navigate(navigateTo, {trigger: true});
      });
    },


    selectMenuItem: function (menuItem) {
      $('.navlist li').removeClass('active');
      if (menuItem) {
        $('.' + menuItem).addClass('active');
      }
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "links" || navigateTo === "link_dropdown") {
        $("#link_dropdown").show(200);
        return;
      }
      if (navigateTo === "tools" || navigateTo === "tools_dropdown") {
        $("#tools_dropdown").show(200);
        return;
      }
      if (navigateTo === "dbselection" || navigateTo === "dbs_dropdown") {
        $("#dbs_dropdown").show(200);
        return;
      }
    },

    hideDropdown: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).closest(".dropdown");
      var navigateTo = tab.attr("id");
      if (navigateTo === "linkDropdown") {
        $("#link_dropdown").hide();
        return;
      }
      if (navigateTo === "toolsDropdown") {
        $("#tools_dropdown").hide();
        return;
      }
      if (navigateTo === "dbSelect") {
        $("#dbs_dropdown").hide();
        return;
      }
    }

  });
}());
