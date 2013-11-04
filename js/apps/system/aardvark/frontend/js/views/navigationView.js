/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window*/

var navigationView = Backbone.View.extend({
  el: '.header',
  initialize: function () {
    var self = this;
  },

  template: templateEngine.createTemplate("navigationView.ejs"),

  render: function() {
    $(this.el).html(this.template.text);
    this.handleSelectNavigation();
    return this;
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
