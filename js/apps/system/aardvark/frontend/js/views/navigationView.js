/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window*/

var navigationView = Backbone.View.extend({
  el: '.header',
  initialize: function () {
    var self = this;
    this.dbSelectionView = new window.DBSelectionView({
      collection: arangoDatabase
    });
  },

  template: templateEngine.createTemplate("navigationView.ejs"),

  render: function() {
    $(this.el).html(this.template.render({}));
    this.handleSelectNavigation();
    this.dbSelectionView.render($("#selectDB2"));
    return this;
  },

  handleResize: function (margin) {
    $('.arango-logo').css('margin-left', margin - 17);
    $("#selectDB2").css('margin-left', margin - 17);
    $('.nav-collapse').css('margin-right', margin - 10);
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
