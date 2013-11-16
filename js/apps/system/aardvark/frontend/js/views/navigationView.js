/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, templateEngine, $, window, arangoDatabase*/
(function() {
  "use strict";
  window.NavigationView = Backbone.View.extend({
    el: '#navigationBar',

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab"
    },

    template: templateEngine.createTemplate("navigationView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({isSystem: window.currentDB.get("isSystem")}));
      return this;
    },

    handleResize: function (margin) {
      $('.arango-logo').css('margin-left', margin - 17);
      $("#dbSelect").css('margin-left', margin - 17);
      $('.nav-collapse').css('margin-right', margin - 10);
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
