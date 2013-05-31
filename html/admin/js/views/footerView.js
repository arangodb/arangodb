/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $*/

var footerView = Backbone.View.extend({
  el: '.footer',
  init: function () {
  },

  template: new EJS({url: 'js/templates/footerView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    return this;
  }

});
