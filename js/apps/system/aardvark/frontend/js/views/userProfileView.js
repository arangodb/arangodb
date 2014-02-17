/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine */
(function() {

  "use strict";

  window.userProfileView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("userProfileView.ejs"),

    events: {
    },

    initialize: function() {
      //this.collection.fetch({async:false});
    },

    render: function(){
      $(this.el).html(this.template.render({}));
      return this;
    }

  });
}());
