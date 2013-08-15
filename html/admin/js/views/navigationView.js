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
    return this;
  },

  handleResize: function () {
    //padding thumbnails 30px
    //padding row 40px
    var containerWidth = $(window).width() - 70;
    //var spanWidth = $('.span3').outerWidth(true);
    var test = $('.span3').outerWidth(true);
    var spanWidth = 242;
    var divider = containerWidth / spanWidth;
    var roundDiv = parseInt(divider, 10);

    //var newWidth = roundDiv*spanWidth+5; OLD
    var newWidth = roundDiv*spanWidth+5;
    var marginWidth = ((containerWidth+30) - newWidth)/2;
    $('#content').width(newWidth);
    $('#content').css('margin-left', marginWidth);
    $('#content').css('margin-right', marginWidth);
    $('.arango-logo').css('margin-left', marginWidth - 17);
    $('.footer-right p').css('margin-right', marginWidth + 10);
    $('.nav-collapse').css('margin-right', marginWidth - 25);
  },

  selectMenuItem: function (menuItem) {
    $('.nav li').removeClass('active');
    if (menuItem) {
      $('.' + menuItem).addClass('active');
    }
  }

});
