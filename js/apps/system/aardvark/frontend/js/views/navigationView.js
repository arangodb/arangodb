/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, templateEngine, $, window, arangoDatabase*/
(function() {
  "use strict";
  window.NavigationView = Backbone.View.extend({
    el: '.header',
    initialize: function () {
      var self = this;
      this.dbSelectionView = new window.DBSelectionView({
        collection: window.arangoDatabase
      });
    },

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab"
    },

    template: templateEngine.createTemplate("navigationView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({}));
      this.dbSelectionView.render($("#selectDB"));
      return this;
    },

    handleResize: function (margin) {
      $('.arango-logo').css('margin-left', margin - 17);
      $("#selectDB").css('margin-left', margin - 17);
      $('.nav-collapse').css('margin-right', margin - 10);
    },

    handleSelectDatabase: function () {
      this.dbSelectionView.render($("#selectDB"));
    },

    navigateBySelect: function() {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    navigateByTab: function(e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      window.App.navigate(navigateTo, {trigger: true});
      e.stopPropagation();
    },
    handleSelectNavigation: function () {
      $("#arangoCollectionSelect").change(function() {
        var navigateTo = $(this).find("option:selected").val();
        window.App.navigate(navigateTo, {trigger: true});
      });
    },


    selectMenuItem: function (menuItem) {
      $('.nav li').removeClass('active');
      if (menuItem) {
        $('.' + menuItem).addClass('active');
      }
    }

  });
}());
