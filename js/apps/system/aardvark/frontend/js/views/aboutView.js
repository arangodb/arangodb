/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $*/

var aboutView = Backbone.View.extend({
  el: '#content',
  init: function () {
  },

  template: new EJS({url: 'js/templates/aboutView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    $.gritter.removeAll();
    return this;
  }

});
