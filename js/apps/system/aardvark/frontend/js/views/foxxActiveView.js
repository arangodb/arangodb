/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, EJS, arangoHelper, _, templateEngine, Joi*/

(function() {
  'use strict';

  window.FoxxActiveView = Backbone.View.extend({
    tagName: 'div',
    className: 'tile',
    template: templateEngine.createTemplate('foxxActiveView.ejs'),

    events: {
      'click' : 'openAppDetailView'
    },

    openAppDetailView: function() {
      // var url = window.location.origin + "/_db/" +
      //           encodeURIComponent(arangoHelper.currentDatabase()) +
      //           this.model.get("mount");
      // var windowRef = window.open(url, this.model.get("title"));
      alert('Michael will insert something here for ' + this.model.get('mount') + '. Stay tuned.');
    },

    render: function(){
      $(this.el).html(this.template.render(this.model));
      return $(this.el);
    }
  });
}());
