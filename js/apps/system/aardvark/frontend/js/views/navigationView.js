/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window*/

var navigationView = Backbone.View.extend({
  el: '.header',
  initialize: function () {
    var self = this;
    $(window).resize(function() {
      self.handleResize();
    });
    this.handleResize();
  },

  template: new EJS({url: 'js/templates/navigationView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    this.handleResize();
    this.handleSelectNavigation();
    return this;
  },

  handleSelectNavigation: function () {
    $("#arangoCollectionSelect").change(function() {
        var navigateTo = $(this).find("option:selected").val();
        window.App.navigate(navigateTo, {trigger: true});
    });
  },

  handleResize: function () {
    var oldWidth = $('#content').width();
    var containerWidth = $(window).width() - 70;
    /*var spanWidth = 242;*/
    var spanWidth = 243;
    var divider = containerWidth / spanWidth;
    var roundDiv = parseInt(divider, 10);
    var newWidth = roundDiv*spanWidth -2;
    var marginWidth = ((containerWidth+30) - newWidth)/2;
    $('#content').width(newWidth)
      .css('margin-left', marginWidth)
      .css('margin-right', marginWidth);
    $('.arango-logo').css('margin-left', marginWidth - 17);
    $('.footer-right p').css('margin-right', marginWidth + 20);
    $('.footer-left p').css('margin-left', marginWidth + 20);
    $('.nav-collapse').css('margin-right', marginWidth - 10);
    if (newWidth !== oldWidth && window.App) {
      window.App.graphView.handleResize(newWidth);
    }
  },

  selectMenuItem: function (menuItem) {
    $('.nav li').removeClass('active');
    if (menuItem) {
      $('.' + menuItem).addClass('active');
    }
  }

});
